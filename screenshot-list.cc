// screenshot-list.cc
// Program to maintain a list of screenshots while playing a game in
// another window.

// See license.txt for copyright and terms of use.

#include "screenshot-list.h"           // this module

#include "dcx.h"                       // DCX
#include "json-util.h"                 // SAVE_KEY_FIELD_CTOR
#include "json.hpp"                    // json::JSON
#include "trace.h"                     // TRACE2, etc.
#include "winapi-util.h"               // WIDE_STRINGIZE, SELECT_RESTORE_OBJECT, GET_AND_RELEASE_HDC

#include <windows.h>                   // Windows API

#include <algorithm>                   // std::clamp
#include <cassert>                     // assert
#include <cstdio>                      // std::{remove, rename}
#include <cstdlib>                     // std::{atoi, getenv, max}
#include <cstring>                     // std::wstrlen
#include <cwchar>                      // std::wcslen
#include <fstream>                     // std::{ifstream, ofstream}
#include <iostream>                    // std::{wcerr, flush}
#include <memory>                      // std::make_unique
#include <sstream>                     // std::wostringstream

using json::JSON;


// Virtual key codes to register as hotkeys.  These are used as the IDs
// when registered.
static int const hotkeyVKs[] = {
  VK_F5,
  VK_UP,
  VK_DOWN,
  VK_DELETE,
};


// Pixel width of the divider separating the list from the larger-size
// display of the selected screenshot.
static int const c_dividerWidth = 3;

// Pixel size of the margin between the list contents and its area edge,
// and between different list elements.
static int const c_listMargin = 5;

// Thickness in pixels of the item highlight frame.
static int const c_listHighlightFrameThickness = 4;

// Margin of the larger selected-shot area.
static int const c_largeShotMargin = 5;

// Number of pixels to vertically scroll the content when the scroll bar
// up/down buttons are clicked.
static int const c_vscrollLineAmount = 20;

// If true, use a hidden buffer to eliminate flickering.
//
// This variable exists just for occasional diagnostic usage.
//
static bool const c_useDoubleBuffer = true;


SLMainWindow::SLMainWindow()
  : m_screenshots(),
    m_listWidth(400),
    m_selectedIndex(-1),
    m_listScroll(0),
    m_hotkeysRegistered(false),
    m_menuBar(nullptr)
{}


SLMainWindow::~SLMainWindow()
{}


void SLMainWindow::captureScreen()
{
  m_screenshots.push_front(std::make_unique<Screenshot>());
  selectItem(0);
  setVScrollInfo();
  invalidateAllPixels();
}


void SLMainWindow::registerHotkeys()
{
  if (!m_hotkeysRegistered) {
    for (int vk : hotkeyVKs) {
      CALL_BOOL_WINAPI(RegisterHotKey,
        m_hwnd,
        vk,                  // id
        0,                   // fsModifiers
        vk);                 // vk
    }

    TRACE2("registered hotkeys");
    m_hotkeysRegistered = true;
    setRegisterHotkeysMenuItemCheckbox();
  }
}


void SLMainWindow::unregisterHotkeys()
{
  if (m_hotkeysRegistered) {
    for (int vk : hotkeyVKs) {
      CALL_BOOL_WINAPI(UnregisterHotKey,
        m_hwnd,
        vk);
    }

    TRACE2("unregistered hotkeys");
    m_hotkeysRegistered = false;
    setRegisterHotkeysMenuItemCheckbox();
  }
}


void SLMainWindow::setHotkeysRegistered(bool r)
{
  if (r != m_hotkeysRegistered) {
    if (r) {
      registerHotkeys();
    }
    else {
      unregisterHotkeys();
    }
  }
}


void SLMainWindow::selectItem(int newIndex)
{
  // Bound the index to the valid range.
  if (m_screenshots.empty()) {
    newIndex = -1;
  }
  else {
    newIndex = std::max(0, newIndex);
    newIndex = std::min((int)m_screenshots.size() - 1, newIndex);
  }

  if (newIndex != m_selectedIndex) {
    m_selectedIndex = newIndex;
    scrollToSelectedIndex();
    setVScrollInfo();
    invalidateAllPixels();
  }
}


void SLMainWindow::boundSelectedIndex()
{
  selectItem(m_selectedIndex);
}


// --------------------------- Serialization ---------------------------
void SLMainWindow::loadFromJSON(json::JSON const &obj)
{
  LOAD_KEY_FIELD(listWidth, data.ToInt());
  LOAD_KEY_FIELD(selectedIndex, data.ToInt());
  LOAD_KEY_FIELD(listScroll, data.ToInt());

  if (obj.hasKey("hotkeysRequired")) {
    setHotkeysRegistered(obj.at("hotkeysRegistered").ToBool());
  }
}


json::JSON SLMainWindow::saveToJSON() const
{
  JSON obj = json::Object();

  {
    JSON shots = json::Array();
    for (auto const &shot : m_screenshots) {
      shots.append(shot->saveToJSON());
    }
    obj["screenshots"] = shots;
  }

  SAVE_KEY_FIELD_CTOR(listWidth);
  SAVE_KEY_FIELD_CTOR(selectedIndex);
  SAVE_KEY_FIELD_CTOR(listScroll);
  SAVE_KEY_FIELD_CTOR(hotkeysRegistered);

  return obj;
}


std::string SLMainWindow::loadFromFile(std::string const &fname)
{
  std::ifstream in(fname, std::ios::binary);
  if (!in) {
    return std::strerror(errno);
  }

  std::ostringstream oss;
  oss << in.rdbuf();

  // This merely reports errors to stderr...
  JSON obj = JSON::Load(oss.str());

  loadFromJSON(obj);

  return "";
}


std::string SLMainWindow::saveToFile(std::string const &fname) const
{
  JSON obj = saveToJSON();

  std::string serialized = obj.dump();

  // Remove an existing backup.
  std::string fnameBak = fname + ".bak";
  std::remove(fnameBak.c_str());

  // Atomically rename any existing file to the backup.
  std::rename(fname.c_str(), fnameBak.c_str());

  // Write the new file.
  std::ofstream out(fname, std::ios::binary);
  if (out) {
    out << serialized << "\n";
    return "";
  }
  else {
    return std::strerror(errno);
  }
}


// ----------------------------- Scrolling -----------------------------
int SLMainWindow::getListContentHeight() const
{
  int y, h;
  getItemVerticalBounds(-1 /*chosenIndex*/, y /*OUT*/, h /*OUT*/);
  return y;
}


void SLMainWindow::getItemVerticalBounds(
  int chosenIndex,
  int &y,          // OUT
  int &h) const    // OUT
{
  y = 0;
  h = 0;

  int currentIndex = 0;
  for (auto const &shot : m_screenshots) {
    int shotHeight = shot->heightForWidth(m_listWidth - c_listMargin*2);

    if (currentIndex == chosenIndex) {
      // We'll say this item's height includes both the top and bottom
      // margins, even though those overlap with adjacent elements.
      h = shotHeight + c_listMargin*2;
      return;
    }

    y += c_listMargin + shotHeight;
    ++currentIndex;
  }

  // If we get here then `chosenIndex` is invalid.  Treat that as a
  // request for the "bounds" of an item beyond the end.
  y += c_listMargin;
}


void SLMainWindow::scrollToSelectedIndex()
{
  if (m_selectedIndex >= 0) {
    // Get the pixel bounds of the selected item.
    int y, h;
    getItemVerticalBounds(m_selectedIndex, y, h);

    // Is the bottom of the selected item below the bottom of the
    // window?
    int windowHeight = getWindowClientHeight(m_hwnd);
    if (y+h > m_listScroll+windowHeight) {
      // Scroll down so we can see the bottom.
      m_listScroll = y+h - windowHeight;
      TRACE2("scrollToSelectedIndex: scroll down:" <<
        " y=" << y <<
        " h=" << h <<
        " windowHeight=" << windowHeight <<
        " listScroll=" << m_listScroll);
    }

    // Is the top of the item above the top of the window?
    if (y < m_listScroll) {
      // Scroll up so we can see the top.
      m_listScroll = y;
      TRACE2("scrollToSelectedIndex: scroll up:" <<
        " y=" << y <<
        " h=" << h <<
        " windowHeight=" << windowHeight <<
        " listScroll=" << m_listScroll);
    }
  }
}


void SLMainWindow::setVScrollInfo()
{
  int windowHeight = getWindowClientHeight(m_hwnd);
  int listContentHeight = getListContentHeight();

  int maxScroll = std::max(0, listContentHeight - windowHeight);
  m_listScroll = std::clamp(m_listScroll, 0, maxScroll);

  SCROLLINFO si{};
  si.cbSize = sizeof(si);
  si.fMask = SIF_DISABLENOSCROLL | SIF_PAGE | SIF_POS | SIF_RANGE;
  si.nMin = 0;

  // Based on the docs, I should not be adding `windowHeight` here, but
  // visually at least, the scroll bar does not behave right otherwise.
  si.nMax = maxScroll + windowHeight;

  si.nPage = windowHeight;
  si.nPos = m_listScroll;
  SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE /*redraw*/);

  TRACE3("setVScrollInfo:" <<
    " contentHeight=" << listContentHeight <<
    " max=" << maxScroll <<
    " page=" << windowHeight <<
    " pos=" << m_listScroll);
}


void SLMainWindow::onVScroll(int request, int newPos)
{
  int windowHeight = getWindowClientHeight(m_hwnd);

  switch (request) {
    case SB_PAGEUP:
      m_listScroll -= windowHeight;
      break;

    case SB_PAGEDOWN:
      m_listScroll += windowHeight;
      break;

    case SB_LINEUP:
      m_listScroll -= c_vscrollLineAmount;
      break;

    case SB_LINEDOWN:
      m_listScroll += c_vscrollLineAmount;
      break;

    case SB_THUMBPOSITION:
    case SB_THUMBTRACK:
      m_listScroll = newPos;
      break;

    default:
      // Ignore any other request.  There are things like SB_BOTTOM but
      // I don't think they are used.  SB_ENDSCROLL gets here too.
      return;
  }

  // This will clamp `m_listScroll`.
  setVScrollInfo();

  invalidateAllPixels();
}


// ------------------------------ Drawing ------------------------------
void SLMainWindow::drawMainWindow(DCX dcx) const
{
  // Clear the window to the background color.
  dcx.fillRectBG();

  HFONT hFont = (HFONT)GetStockObject(SYSTEM_FONT);
  SELECT_RESTORE_OBJECT(dcx.hdc, hFont);

  // Split the window into three regions.
  std::vector<DCX> dcxColumns = dcx.splitHorizontallyFromRight(
    std::vector<int>{c_dividerWidth, m_listWidth});

  // Draw the window elements.
  drawLargeShot(dcxColumns[0]);
  drawDivider(dcxColumns[1]);
  drawShotList(dcxColumns[2]);
}


void SLMainWindow::drawDivider(DCX dcx) const
{
  dcx.fillRectSysColor(COLOR_GRAYTEXT);
}


void SLMainWindow::drawLargeShot(DCX dcx) const
{
  dcx.shrinkByMargin(c_largeShotMargin);

  if (m_screenshots.empty() || m_selectedIndex < 0) {
    dcx.textOut(L"No screenshot selected");
  }
  else {
    // Draw timestamp of selected screenshot.
    Screenshot const *sel = m_screenshots.at(m_selectedIndex).get();
    dcx.textOut_moveTop(sel->m_fname);

    // Draw a larger version of the selected screenshot.
    sel->drawToDCX_autoHeight(dcx);
  }
}


void SLMainWindow::drawShotList(DCX dcx) const
{
  // Implement scrolling by moving our cursor into negative territory.
  dcx.y = -m_listScroll;

  // Pretend the height we will draw to is also taller by that amount,
  // so exchausing `h` means we crossed the window's lower bounds.
  dcx.h += m_listScroll;

  dcx.shrinkByMargin(c_listMargin);

  // Draw the screenshots.
  if (m_screenshots.empty()) {
    dcx.textOut(L"No screenshots");
  }
  else {
    int currentIndex = 0;
    for (auto const &screenshot : m_screenshots) {
      int shotHeight = screenshot->heightForWidth(dcx.w);

      if (currentIndex == m_selectedIndex) {
        // Compute the highlight rectangle by expanding what we will
        // draw as the screenshot.
        DCX dcxHighlight(dcx);
        dcxHighlight.h = shotHeight;
        dcxHighlight.shrinkByMargin(-c_listHighlightFrameThickness);

        // Draw it first so the shot covers most of the highlight
        // rectangle, leaving just a rectangular frame.
        dcxHighlight.fillRectSysColor(COLOR_HIGHLIGHT);
      }

      screenshot->drawToDCX_autoHeight(dcx);

      dcx.moveTopBy(shotHeight + c_listMargin);
      ++currentIndex;

      if (dcx.h <= 0) {
        break;
      }
    }
  }
}


void SLMainWindow::onPaint()
{
  PAINTSTRUCT ps;
  HDC hdc;
  CALL_HANDLE_WINAPI(hdc, BeginPaint, m_hwnd, &ps);

  RECT rcClient = getWindowClientArea(m_hwnd);

  if (c_useDoubleBuffer) {
    // Make an in-memory DC for double buffering to avoid flicker.
    BitmapDC memDC(hdc, rcClient.right, rcClient.bottom);

    // Actual drawing.
    {
      DCX dcxWindow(memDC.getDC(), m_hwnd);
      drawMainWindow(dcxWindow);
    }

    // Copy from hidden buffer.
    CALL_BOOL_WINAPI(BitBlt,
      hdc,                               // hdcDest
      0, 0,                              // xDest, yDest
      rcClient.right,                    // wDest
      rcClient.bottom,                   // hDest
      memDC.getDC(),                     // hdcSrc
      0, 0,                              // xSrc, ySrc
      SRCCOPY);                          // rop
  }

  else {
    DCX dcxWindow(hdc, m_hwnd);
    drawMainWindow(dcxWindow);
  }

  EndPaint(m_hwnd, &ps);
}


// -------------------------- Keyboard input ---------------------------
void SLMainWindow::onHotKey(WPARAM id, WPARAM fsModifiers, WPARAM vk)
{
  TRACE2(L"hotkey:"
         " id=" << id <<
         " fsModifiers=" << fsModifiers <<
         " vk=" << vk);

  // Treat hotkeys the same as regular keypresses.  This way we can
  // still handle them when the hotkeys are not registered.
  onKeyPress(vk);
}


bool SLMainWindow::onKeyPress(int vk)
{
  TRACE2(L"onKeyPress:" << vk);

  switch (vk) {
    case 'Q':
      // Q to quit.
      TRACE2(L"Saw Q keypress.");
      PostMessage(m_hwnd, WM_CLOSE, 0, 0);
      return true;

    case VK_F5:
      // Take a new screenshot.
      captureScreen();
      break;

    case VK_DELETE:
      // Discard the selected screenshot.
      if (!m_screenshots.empty() && m_selectedIndex >= 0) {
        m_screenshots.erase(m_screenshots.cbegin() + m_selectedIndex);
        boundSelectedIndex();
        setVScrollInfo();
        invalidateAllPixels();
      }
      break;

    case VK_UP:
      selectItem(m_selectedIndex-1);
      break;

    case VK_DOWN:
      selectItem(m_selectedIndex+1);
      return true;
  }

  // Not handled.
  return false;
}


// ------------------------------- Menu --------------------------------
// Menu IDs.
enum {
  // File
  IDM_SAVE = 1,
  IDM_QUIT,

  // Options
  IDM_REGISTER_HOTKEYS,

  // Help
  IDM_ABOUT,
};


void SLMainWindow::createAppMenu()
{
  m_menuBar = createMenu();

  // File
  {
    HMENU menu = createMenu();

    appendMenuW(menu, MF_STRING, IDM_SAVE, L"&Save to shots/list.json");
    appendMenuW(menu, MF_STRING, IDM_QUIT, L"&Quit");

    appendMenuW(m_menuBar, MF_POPUP, (UINT_PTR)menu, L"&File");
  }

  // Options
  {
    HMENU menu = createMenu();

    appendMenuW(menu, MF_STRING, IDM_REGISTER_HOTKEYS, L"Register &hotkeys");

    appendMenuW(m_menuBar, MF_POPUP, (UINT_PTR)menu, L"&Options");
  }

  // Help
  {
    HMENU menu = createMenu();

    appendMenuW(menu, MF_STRING, IDM_ABOUT, L"&About...");

    appendMenuW(m_menuBar, MF_POPUP, (UINT_PTR)menu, L"&Help");
  }

  setMenu(m_hwnd, m_menuBar);
}


void SLMainWindow::fileSave()
{
  createDirectoryIfNeeded(L"shots");
  std::string error = saveToFile("shots/list.json");
  if (!error.empty()) {
    MessageBox(m_hwnd,
      toWideString(error).c_str(),
      L"Error saving shots/list.json",
      MB_OK);
  }
  else {
    TRACE2(L"wrote shots/list.json");
  }
}


void SLMainWindow::onCommand(int menuId)
{
  TRACE2(L"onCommand: " << menuId);

  switch (menuId) {
    case IDM_SAVE:
      fileSave();
      break;

    case IDM_QUIT:
      PostMessage(m_hwnd, WM_CLOSE, 0, 0);
      break;

    case IDM_REGISTER_HOTKEYS:
      setHotkeysRegistered(!m_hotkeysRegistered);
      break;

    case IDM_ABOUT:
      MessageBox(m_hwnd,

        L"Screenshot List v1.0\n"
        L"(c) 2024 Scott McPeak\n"
        L"Licensed under the MIT open source license; see license.txt\n"
        L"Icon: freepik.com/icon/camera_1042390\n",

        L"About Screenshot List",
        MB_OK);
      break;
  }
}


void SLMainWindow::setRegisterHotkeysMenuItemCheckbox()
{
  // This doesn't have a useful error return.
  CheckMenuItem(m_menuBar, IDM_REGISTER_HOTKEYS,
    MF_BYCOMMAND | (m_hotkeysRegistered? MF_CHECKED : MF_UNCHECKED));
}


// ------------------------ Messages generally -------------------------
LRESULT CALLBACK SLMainWindow::handleMessage(
  UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg) {
    case WM_CREATE:
      // Set the window icon.
      {
        HICON icon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(1));
        SendMessage(m_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icon);
        SendMessage(m_hwnd, WM_SETICON, ICON_BIG, (LPARAM)icon);
      }

      createAppMenu();
      registerHotkeys();
      setVScrollInfo();

      return 0;

    case WM_DESTROY:
      TRACE2(L"received WM_DESTROY");

      unregisterHotkeys();

      PostQuitMessage(0);
      return 0;

    case WM_PAINT:
      onPaint();
      return 0;

    case WM_HOTKEY:
      onHotKey(wParam, LOWORD(lParam), HIWORD(lParam));
      return 0;

    case WM_KEYDOWN:
      if (onKeyPress(wParam)) {
        // Handled.
        return 0;
      }
      break;

    case WM_VSCROLL:
      onVScroll(LOWORD(wParam), HIWORD(wParam));
      return 0;

    case WM_SIZE:
      // The default behavior will only repaint newly-exposed areas, but
      // I want the active screenshot to be stretched to fill the
      // window, so I need all of it repainted.
      invalidateAllPixels();
      setVScrollInfo();
      return 0;

    case WM_COMMAND:
      onCommand(LOWORD(wParam));
      return 0;
  }

  return BaseWindow::handleMessage(uMsg, wParam, lParam);
}


// ------------------------------ Startup ------------------------------
// If `envvar` is set, return its value as an integer.  Otherwise return
// `defaultValue`.
static int envIntOr(char const *envvar, int defaultValue)
{
  if (char const *value = std::getenv(envvar)) {
    return std::atoi(value);
  }
  else {
    return defaultValue;
  }
}


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PWSTR pCmdLine, int nCmdShow)
{
  // Elsewhere, I rely on the assumption that I can get the module
  // handle using `GetModuleHandle`.
  assert(hInstance == GetModuleHandle(nullptr));

  // Configure tracing level, with default of 1.
  g_tracingLevel = envIntOr("TRACE", 1);

  // Create the window.
  SLMainWindow mainWindow;
  CreateWindowExWArgs cw;
  cw.m_lpWindowName = L"Screenshot List";
  cw.m_x       = 200;
  cw.m_y       = 100;
  cw.m_nWidth  = 1200;
  cw.m_nHeight = 800;
  cw.m_dwStyle = WS_OVERLAPPEDWINDOW | WS_VSCROLL;
  mainWindow.createWindow(cw);

  TRACE2(L"Calling ShowWindow");
  ShowWindow(mainWindow.m_hwnd, nCmdShow);

  // Run the message loop.

  MSG msg = { };
  while (GetMessage(&msg, NULL, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  TRACE2(L"Returning from main");
  return 0;
}


// EOF
