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

  // Width of the screenshot list in pixels.
  int m_listWidth;

  // Index of the selected list item, or -1 for none.
  int m_selectedIndex;

public:      // methods
  GTLMainWindow();
  ~GTLMainWindow();

  // Take a screen capture and prepend it to the "to do" list.
  void captureScreen();

  // register/unregister our global hotkeys.
  void registerHotkeys();
  void unregisterHotkeys();

  // Select the item at `newIndex`.  If it is out of range, the index is
  // set to the appropriate endpoint, or -1 if there are no list
  // elements.  Then the window is redrawn if the selection has changed.
  void selectItem(int newIndex);

  // If `m_selectedIndex` is out of bounds, correct that.
  void boundSelectedIndex();

  // Draw the divider.
  void drawDivider(DCX dcx) const;

  // Draw the large screenshot of the slected element (if any) on the
  // left side of the divider.
  void drawLargeShot(DCX dcx) const;

  // Draw the list of all shots on the right side.
  void drawShotList(DCX dcx) const;

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
