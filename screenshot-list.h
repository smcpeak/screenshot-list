// screenshot-list.h
// Main window class of the screenshot list app.

// See license.txt for copyright and terms of use.

#ifndef SCREENSHOT_LIST_H
#define SCREENSHOT_LIST_H

#include "base-window.h"               // BaseWindow
#include "json-fwd.h"                  // json::JSON
#include "screenshot.h"                // Screenshot

#include <windows.h>                   // Windows API

#include <deque>                       // std::deque
#include <memory>                      // std::unique_ptr


// Main window of the screenshot list app.
class SLMainWindow : public BaseWindow {
public:      // model data (serialized to JSON)
  // Sequence of screenshots, most recent first.
  std::deque<std::unique_ptr<Screenshot>> m_screenshots;

  // Width of the screenshot list in pixels.
  int m_listWidth;

  // Index of the selected list item, or -1 for none.
  int m_selectedIndex;

  // Number of pixels the list is scrolled, where 0 means we see the top
  // pixel of the list.
  int m_listScroll;

  // If true, the hotkeys have been registered.
  bool m_hotkeysRegistered;

public:      // ui data (ephemeral)
  // The menu bar of the main window.  It is conceptually owned by this
  // object, but because it is assigned as the window's menu, the window
  // destroys it automatically on shutdown.
  HMENU m_menuBar;

public:      // methods
  SLMainWindow();
  ~SLMainWindow();

  // Take a screen capture and prepend it to the "to do" list.
  void captureScreen();

  // Register/unregister our global hotkeys.
  void registerHotkeys();
  void unregisterHotkeys();

  // If `r`, then register the hotkeys; otherwise, unregister them.
  // Does nothing if `r` equals `m_hotkeysRegistered`.
  void setHotkeysRegistered(bool r);

  // Select the item at `newIndex`.  If it is out of range, the index is
  // set to the appropriate endpoint, or -1 if there are no list
  // elements.  Then the window is redrawn if the selection has changed.
  void selectItem(int newIndex);

  // If `m_selectedIndex` is out of bounds, correct that.
  void boundSelectedIndex();

  // -------------------------- Serialization --------------------------
  // De/serialize as JSON.
  void loadFromJSON(json::JSON const &obj);
  json::JSON saveToJSON() const;

  // Load settings from the named file.  Return an empty string on
  // success, and an error message otherwise.
  std::string loadFromFile(std::string const &fname);

  // Save the settings.  Return a non-empty error message on failure.
  std::string saveToFile(std::string const &fname) const;

  // ---------------------------- Scrolling ----------------------------
  // Return the number of pixels that the list would occupy if the
  // window were infinitely tall.
  int getListContentHeight() const;

  // In an infinite-height window with no scrolling, where would item
  // `chosenIndex` be placed vertically?  If `chosenIndex` is not the
  // index of any item, then set `y` to the total content height and `h`
  // to zero, thus representing the bounds of a virtual past-the-end
  // empty item.
  void getItemVerticalBounds(int chosenIndex,
    int &y /*OUT*/, int &h /*OUT*/) const;

  // Set `m_listScroll` so that the entire selected item is in the
  // visible range.  If the window is too short to show the whole thing,
  // place its top at the top of the window.
  void scrollToSelectedIndex();

  // Set the vertical scroll bar to reflect the current state.  Also
  // clamp `m_listScroll`.
  void setVScrollInfo();

  // Respond to scroll bar manipulation.  `request` is one of the
  // `SB_XXX` constants, and `newPos` is the new scroll position if
  // `request` is `SB_THUMBPOSITION` or `SB_THUMBTRACK`, and meaningless
  // otherwise.
  void onVScroll(int request, int newPos);

  // ----------------------------- Drawing -----------------------------
  // Draw the main window clinet area.
  void drawMainWindow(DCX dcx) const;

  // Draw the divider.
  void drawDivider(DCX dcx) const;

  // Draw the large screenshot of the slected element (if any) on the
  // left side of the divider.
  void drawLargeShot(DCX dcx) const;

  // Draw the list of all shots on the right side.
  void drawShotList(DCX dcx) const;

  // Handle `WM_PAINT`.
  void onPaint();

  // ------------------------- Keyboard input --------------------------
  // Handle `WM_HOTKEY`.
  void onHotKey(WPARAM id, WPARAM fsModifiers, WPARAM vk);

  // Handle `WM_KEYDOWN`.  Return true if handled.
  bool onKeyPress(int vk);

  // ------------------------------ Menu -------------------------------
  // Create the application menu bar and associate it with the window.
  void createAppMenu();

  // File|Save menu action.
  void fileSave();

  // Handle menu command `menuId`.
  void onCommand(int menuId);

  // Set the checkmark state of the `IDM_REGISTER_HOTKEYS` menu item
  // based on the current value of `m_hotkeysRegistered`.
  void setRegisterHotkeysMenuItemCheckbox();

  // ----------------------- Messages generally ------------------------
  // BaseWindow methods.
  virtual LRESULT handleMessage(
    UINT uMsg, WPARAM wParam, LPARAM lParam) override;
};


#endif // SCREENSHOT_LIST_H
