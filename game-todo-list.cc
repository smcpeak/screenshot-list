// game-todo-list.cc
// Program to maintain a TODO list while playing a game in another window.

// See license.txt for copyright and terms of use.

#include "game-todo-list.h"            // this module

#include "winapi-util.h"               // WIDE_STRINGIZE, SELECT_RESTORE_OBJECT, GET_AND_RELEASE_HDC

#include <windows.h>                   // Windows API

#include <cassert>                     // assert
#include <cstdlib>                     // std::{atoi, getenv}
#include <cstring>                     // std::wstrlen
#include <cwchar>                      // std::wcslen
#include <iostream>                    // std::{wcerr, flush}
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


// Identifiers for the registered hotkeys.
static int const HOTKEY_ID_F5 = 1;
static int const HOTKEY_ID_UP = 2;


GTLMainWindow::GTLMainWindow()
  : m_screenshot()
{}


GTLMainWindow::~GTLMainWindow()
{}


void GTLMainWindow::captureScreen()
{
  m_screenshot.reset(new Screenshot());
  invalidateAllPixels();
}


void GTLMainWindow::onPaint()
{
  PAINTSTRUCT ps;
  HDC hdc;
  CALL_HANDLE_WINAPI(hdc, BeginPaint, m_hwnd, &ps);

  // Open a scope so selected objects can be restored at scope exit.
  {
    FillRect(hdc, &ps.rcPaint, (HBRUSH) (COLOR_WINDOW+1));

    if (m_screenshot) {
      CompatibleHDC memDC(hdc);

      // Select the screenshot into the memory DC so the bitmap will act
      // as its data source.
      SELECT_RESTORE_OBJECT(memDC.m_hdc, m_screenshot->m_bitmap);

      // The MS docs claim this is the "best" stretch mode.
      SetStretchBltMode(memDC.m_hdc, HALFTONE);

      // Region of our window to fill with the screenshot.
      RECT rcClient;
      CALL_BOOL_WINAPI(GetClientRect, m_hwnd, &rcClient);

      // Copy the screenshot into our window with stretching.
      //
      // TODO: Preserve the aspect ratio!
      //
      CALL_BOOL_WINAPI(StretchBlt,
        hdc,                               // hdcDest
        0, 0,                              // xDest, yDest
        rcClient.right,                    // wDest
        rcClient.bottom,                   // hDest
        memDC.m_hdc,                       // hdcSrc
        0, 0,                              // xSrc, ySrc
        m_screenshot->m_width,             // wSrc
        m_screenshot->m_height,            // hSrc
        SRCCOPY);                          // rop
    }

    else {
      HFONT hFont = (HFONT)GetStockObject(SYSTEM_FONT);
      SELECT_RESTORE_OBJECT(hdc, hFont);

      wchar_t const *msg = L"No screenshot";
      CALL_BOOL_WINAPI(TextOut, hdc, 10, 10, msg, std::wcslen(msg));
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

  if (id == HOTKEY_ID_F5) {
    captureScreen();
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

      // Register the hotkeys.
      CALL_BOOL_WINAPI(RegisterHotKey,
        m_hwnd,
        HOTKEY_ID_F5,        // id
        0,                   // fsModifiers
        VK_F5);              // vk
      CALL_BOOL_WINAPI(RegisterHotKey,
        m_hwnd,
        HOTKEY_ID_UP,        // id
        0,                   // fsModifiers
        VK_UP);              // vk

      return 0;

    case WM_DESTROY:
      TRACE2(L"received WM_DESTROY");

      CALL_BOOL_WINAPI(UnregisterHotKey,
        m_hwnd,
        HOTKEY_ID_F5);
      CALL_BOOL_WINAPI(UnregisterHotKey,
        m_hwnd,
        HOTKEY_ID_UP);

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
  cw.m_lpWindowName = L"Game TODO List";
  cw.m_x       = 200;
  cw.m_y       = 200;
  cw.m_nWidth  = 400;
  cw.m_nHeight = 400;
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
