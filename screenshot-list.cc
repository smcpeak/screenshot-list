// screenshot-list.cc
// Program to maintain a list of screenshots while playing a game in
// another window.

// See license.txt for copyright and terms of use.

#include "screenshot-list.h"           // this module

#include "dcx.h"                       // DCX
#include "trace.h"                     // TRACE2, etc.
#include "winapi-util.h"               // WIDE_STRINGIZE, SELECT_RESTORE_OBJECT, GET_AND_RELEASE_HDC

#include <windows.h>                   // Windows API

#include <algorithm>                   // std::clamp
#include <cassert>                     // assert
#include <cstdlib>                     // std::{atoi, getenv, max}
#include <cstring>                     // std::wstrlen
#include <cwchar>                      // std::wcslen
#include <iostream>                    // std::{wcerr, flush}
#include <memory>                      // std::make_unique
#include <sstream>                     // std::wostringstream


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


GTLMainWindow::GTLMainWindow()
  : m_screenshots(),
    m_listWidth(400),
    m_selectedIndex(-1),
    m_listScroll(0)
{}


GTLMainWindow::~GTLMainWindow()
{}


void GTLMainWindow::captureScreen()
{
  m_screenshots.push_front(std::make_unique<Screenshot>());
  selectItem(0);
  setVScrollInfo();
  invalidateAllPixels();
}


void GTLMainWindow::registerHotkeys()
{
  for (int vk : hotkeyVKs) {
    CALL_BOOL_WINAPI(RegisterHotKey,
      m_hwnd,
      vk,                  // id
      0,                   // fsModifiers
      vk);                 // vk
  }
}


void GTLMainWindow::unregisterHotkeys()
{
  for (int vk : hotkeyVKs) {
    CALL_BOOL_WINAPI(UnregisterHotKey,
      m_hwnd,
      vk);
  }
}


void GTLMainWindow::selectItem(int newIndex)
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


void GTLMainWindow::boundSelectedIndex()
{
  selectItem(m_selectedIndex);
}


// ----------------------------- Scrolling -----------------------------
int GTLMainWindow::getListContentHeight() const
{
  int y, h;
  getItemVerticalBounds(-1 /*chosenIndex*/, y /*OUT*/, h /*OUT*/);
  return y;
}


void GTLMainWindow::getItemVerticalBounds(
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


void GTLMainWindow::scrollToSelectedIndex()
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


void GTLMainWindow::setVScrollInfo()
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


void GTLMainWindow::onVScroll(int request, int newPos)
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
void GTLMainWindow::drawMainWindow(DCX dcx) const
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


void GTLMainWindow::drawDivider(DCX dcx) const
{
  dcx.fillRectSysColor(COLOR_GRAYTEXT);
}


void GTLMainWindow::drawLargeShot(DCX dcx) const
{
  dcx.shrinkByMargin(c_largeShotMargin);

  if (m_screenshots.empty() || m_selectedIndex < 0) {
    dcx.textOut(L"No screenshot selected");
  }
  else {
    // Draw timestamp of selected screenshot.
    Screenshot const *sel = m_screenshots.at(m_selectedIndex).get();
    dcx.textOut_moveTop(sel->m_timestamp);

    // Draw a larger version of the selected screenshot.
    sel->drawToDCX_autoHeight(dcx);
  }
}


void GTLMainWindow::drawShotList(DCX dcx) const
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


void GTLMainWindow::onPaint()
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
void GTLMainWindow::onHotKey(WPARAM id, WPARAM fsModifiers, WPARAM vk)
{
  TRACE2(L"hotkey:"
         " id=" << id <<
         " fsModifiers=" << fsModifiers <<
         " vk=" << vk);

  switch (id) {
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
      break;
  }
}


bool GTLMainWindow::onKeyDown(WPARAM wParam, LPARAM lParam)
{
  TRACE2(L"onKeyDown:" << std::hex <<
         TRVAL(wParam) << TRVAL(lParam) << std::dec);

  switch (wParam) {
    case 'Q':
      // Q to quit.
      TRACE2(L"Saw Q keypress.");
      PostMessage(m_hwnd, WM_CLOSE, 0, 0);
      return true;
  }

  // Not handled.
  return false;
}


// ------------------------ Messages generally -------------------------
LRESULT CALLBACK GTLMainWindow::handleMessage(
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
      if (onKeyDown(wParam, lParam)) {
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
  GTLMainWindow mainWindow;
  CreateWindowExWArgs cw;
  cw.m_lpWindowName = L"Game To-Do List";
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
