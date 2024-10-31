// game-todo-list.h
// Main window class of the "to do" list app.

// See license.txt for copyright and terms of use.

#ifndef GAME_TODO_LIST_H
#define GAME_TODO_LIST_H

#include "base-window.h"               // BaseWindow

#include <windows.h>                   // Windows API


// Main window of the "to do" app.
class GTLMainWindow : public BaseWindow {
public:      // data
  // (None yet.)

public:      // methods
  GTLMainWindow();

  // Handle `WM_PAINT`.
  void onPaint();

  // Handle `WM_KEYDOWN`.  Return true if handled.
  bool onKeyDown(WPARAM wParam, LPARAM lParam);

  // BaseWindow methods.
  virtual LRESULT handleMessage(
    UINT uMsg, WPARAM wParam, LPARAM lParam) override;
};


#endif // GAME_TODO_LIST_H
