// game-todo-list.cc
// Program to maintain a TODO list while playing a game in another window.

// See license.txt for copyright and terms of use.

#include "game-todo-list.h"            // this module

#include "winapi-util.h"               // WIDE_STRINGIZE, SELECT_RESTORE_OBJECT, GET_AND_RELEASE_HDC

#include <windows.h>                   // Windows API

#include <cassert>                     // assert
#include <cstdlib>                     // std::{atoi, getenv, max}
#include <cstring>                     // std::wstrlen
#include <cwchar>                      // std::wcslen
#include <iostream>                    // std::{wcerr, flush}
#include <memory>                      // std::make_unique
#include <sstream>                     // std::wostringstream


// Level of diagnostics to print.
//
//   1: API call failures.
//
//   2: Information about messages, etc., of low volume.
//
//   3: Higher-volume messages, e.g., relating to mouse movement.
//
// The default value is not used, as `wWinMain` overwrites it.
//
int g_tracingLevel = 1;


// Write a diagnostic message.
#define TRACE(level, msg)           \
  if (g_tracingLevel >= (level)) {  \
    std::wcerr << msg << std::endl; \
  }

#define TRACE1(msg) TRACE(1, msg)
#define TRACE2(msg) TRACE(2, msg)
#define TRACE3(msg) TRACE(3, msg)

// Add the value of `expr` to a chain of outputs using `<<`.
#define TRVAL(expr) L" " WIDE_STRINGIZE(expr) L"=" << (expr)


// Virtual key codes to register as hotkeys.  These are used as the IDs
// when registered.
static int const hotkeyVKs[] = {
  VK_F5,
  VK_UP,
  VK_DELETE,
};


// Pixel width of the divider separating the list from the larger-size
// display of the selected screenshot.
static int const c_dividerWidth = 3;

// Pixel size of the margin between the list contents and its area edge,
// and between different list elements.
static int const c_listMargin = 5;

// Margin of the larger selected-shot area.
static int const c_largeShotMargin = 5;


GTLMainWindow::GTLMainWindow()
  : m_screenshots(),
    m_listWidth(400),
    m_selectedIndex(0)
{}


GTLMainWindow::~GTLMainWindow()
{}


void GTLMainWindow::captureScreen()
{
  m_screenshots.push_front(std::make_unique<Screenshot>());
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


void GTLMainWindow::onPaint()
{
  PAINTSTRUCT ps;
  HDC hdc;
  CALL_HANDLE_WINAPI(hdc, BeginPaint, m_hwnd, &ps);

  // Open a scope so selected objects can be restored at scope exit.
  {
    RECT rcClient;
    CALL_BOOL_WINAPI(GetClientRect, m_hwnd, &rcClient);

    // Clear the window to the background color.
    //
    // TODO: This causes flickering because some pixels are drawn more
    // than once.  Ideally I would fix that.
    //
    fillRectBG(hdc, 0, 0, rcClient.right, rcClient.bottom);

    HFONT hFont = (HFONT)GetStockObject(SYSTEM_FONT);
    SELECT_RESTORE_OBJECT(hdc, hFont);

    // Left edge of the screenshot list.
    int x = std::max(rcClient.right - m_listWidth, 0L);

    // Draw the divider.
    fillRectSysColor(hdc,
      x - c_dividerWidth, 0,
      c_dividerWidth, rcClient.bottom,
      COLOR_GRAYTEXT);

    // Move our "cursor" into the content part of the list.
    x += c_listMargin;
    int y = c_listMargin;

    // Draw the screenshots.
    if (!m_screenshots.empty()) {
      {
        // Width of the list content area.
        int innerWidth = m_listWidth - c_listMargin*2;

        for (auto const &screenshot : m_screenshots) {
          y += screenshot->drawToDC_autoHeight(hdc, x, y, innerWidth);
          y += c_listMargin;
        }
      }

      // Reset the cursor for use in the main area.
      x = c_largeShotMargin;
      y = c_largeShotMargin;

      // Width of main area.
      int w = rcClient.right - m_listWidth
                             - c_dividerWidth
                             - c_largeShotMargin*2;

      // Draw timestamp of selected screenshot.
      Screenshot const *sel = m_screenshots.at(m_selectedIndex).get();
      y += textOut(hdc, x, y, sel->m_timestamp).cy;

      // Draw a larger version of the selected screenshot.
      sel->drawToDC_autoHeight(hdc, x, y, w);
    }

    else {
      textOut(hdc, x, y, L"No screenshots");
    }
  }

  EndPaint(m_hwnd, &ps);
}


void GTLMainWindow::onHotKey(WPARAM id, WPARAM fsModifiers, WPARAM vk)
{
  TRACE2(L"hotkey:"
         " id=" << id <<
         " fsModifiers=" << fsModifiers <<
         " vk=" << vk);

  if (id == VK_F5) {
    // Take a new screenshot.
    captureScreen();
  }

  if (id == VK_DELETE) {
    // Discard the top-most screenshot.
    if (!m_screenshots.empty()) {
      m_screenshots.pop_front();
      invalidateAllPixels();
    }
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

    case WM_SIZE:
      // The default behavior will only repaint newly-exposed areas, but
      // I want the active screenshot to be stretched to fill the
      // window, so I need all of it repainted.
      invalidateAllPixels();
      return 0;
  }

  return BaseWindow::handleMessage(uMsg, wParam, lParam);
}


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
  cw.m_y       = 200;
  cw.m_nWidth  = 1200;
  cw.m_nHeight = 800;
  cw.m_dwStyle = WS_OVERLAPPEDWINDOW;
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
