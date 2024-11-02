// screenshot.h
// Class `Screenshot` representing a single in-game screenshot.

#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include "dcx.h"                       // DCX
#include "winapi-util.h"               // NO_OBJECT_COPIES

#include <string>                      // std::wstring

#include <windows.h>                   // HBITMAP


// A single in-game screenshot, and some miscellaneous related data.
class Screenshot {
  NO_OBJECT_COPIES(Screenshot);

public:      // data
  // The screenshot bitmap, as a GDI object compatible with the DC
  // obtained from `GetDC(null)` (representing the screen).
  HBITMAP m_bitmap;

  // Size of that bitmap in pixels.
  int m_width;
  int m_height;

  // Name of the file to which the image has been saved.  The name
  // format is "YYYY-MM-DDThh-mm-ssU.bmp" format, where 'T' is literal,
  // and 'U' is a suffix string appended to make the name unique when
  // needed.
  std::wstring m_fname;

public:
  // Capture the current screen contents.
  Screenshot();

  ~Screenshot();

  // Draw the bitmap to `hdc` at the specified coordinates.  This
  // preserves the source image aspect ratio, drawing window background
  // color bars on the sides as needed to fill the space.
  void drawToDC(HDC hdc, int x, int y, int w, int h) const;

  // Like `drawToDC`, but calculate the height for the given width, and
  // return that height.
  int drawToDC_autoHeight(HDC hdc, int x, int y, int w) const;

  // Like `drawToDC_autoHeight`, but use the data in `dcx`.
  int drawToDCX_autoHeight(DCX const &dcx) const;

  // If we want to draw the screenshot within a column of width 'w'
  // pixels, return the corresponding pixel height that will allow the
  // image to be shown with its proper aspect ratio.
  int heightForWidth(int w) const;

  // Choose a unique value for `m_fname`.
  void chooseFileName();

  // Write the image to a file in BMP format.
  void writeToBMPFile(std::wstring const &fname) const;
};


#endif // SCREENSHOT_H
