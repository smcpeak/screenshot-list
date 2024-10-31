// screenshot.h
// Class `Screenshot` representing a single in-game screenshot.

#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include "winapi-util.h"               // NO_OBJECT_COPIES

#include <string>                      // std::wstring

#include <windows.h>                   // HBITMAP


// A single in-game screenshot, and some miscellaneous related data.
class Screenshot {
  NO_OBJECT_COPIES(Screenshot);

public:      // data
  // The screenshot bitmap.
  HBITMAP m_bitmap;

  // Size of that bitmap in pixels.
  int m_width;
  int m_height;

  // Timestamp when the shot was taken, in "YYYY-MM-DD hh:mm" format.
  std::wstring m_timestamp;

public:
  // Capture the current screen contents.
  Screenshot();

  ~Screenshot();

  // Draw the bitmap to `hdc` at the specified coordinates.
  void drawToDC(HDC hdc, int x, int y, int w, int h);
};


#endif // SCREENSHOT_H
