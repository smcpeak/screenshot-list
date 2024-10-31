// game-todo-list.h
// Main window class of the "to do" list app.

// See license.txt for copyright and terms of use.

#ifndef GAME_TODO_LIST_H
#define GAME_TODO_LIST_H

#include "base-window.h"               // BaseWindow
#include "screenshot.h"                // Screenshot

#include <windows.h>                   // Windows API

#include <deque>                       // std::deque
#include <memory>                      // std::unique_ptr


// Main window of the "to do" app.
class GTLMainWindow : public BaseWindow {
public:      // data
  // Sequence of screenshots, most recent first.
  std::deque<std::unique_ptr<Screenshot>> m_screenshots;

public:      // methods
  GTLMainWindow();
  ~GTLMainWindow();

  // Take a screen capture and prepend it to the "to do" list.
  void captureScreen();

  // register/unregister our global hotkeys.
  void registerHotkeys();
  void unregisterHotkeys();

  // Handle `WM_PAINT`.
  void onPaint();

  // Handle `WM_HOTKEY`.
  void onHotKey(WPARAM id, WPARAM fsModifiers, WPARAM vk);

  // Handle `WM_KEYDOWN`.  Return true if handled.
  bool onKeyDown(WPARAM wParam, LPARAM lParam);

  // BaseWindow methods.
  virtual LRESULT handleMessage(
    UINT uMsg, WPARAM wParam, LPARAM lParam) override;
};


#endif // GAME_TODO_LIST_H
