// screenshot.cc
// Code for `screenshot.h`.

#include "screenshot.h"                // this module

#include "winapi-util.h"               // CompatibleDC, etc.

#include <cwchar>                      // std::swprintf
#include <string>                      // std::wstring

#include <windows.h>                   // GetLocalTime, etc.


// Return the current date/time, in the local time zone, in
// "YYYY-MM-DD hh:mm:ss" format.
static std::wstring getLocaltime()
{
  SYSTEMTIME st;
  GetLocalTime(&st);

  // 20 would be enough for 4-digit years, but years do not technically
  // have an upper bound (although the Windows API limits them to ~30k).
  wchar_t buf[30];

  swprintf(buf, sizeof(buf) / sizeof(buf[0]),
    L"%04d-%02d-%02d %02d:%02d:%02d",
    (int)st.wYear,
    (int)st.wMonth,
    (int)st.wDay,
    (int)st.wHour,
    (int)st.wMinute,
    (int)st.wSecond);

  return std::wstring(buf);
}


Screenshot::Screenshot()
  : m_bitmap(nullptr),
    m_width(GetSystemMetrics(SM_CXSCREEN)),
    m_height(GetSystemMetrics(SM_CYSCREEN)),
    m_timestamp(getLocaltime())
{
  GET_AND_RELEASE_HDC(hdcScreen, NULL);

  CompatibleHDC memDC(hdcScreen);

  // Create a compatible bitmap from the screen DC.
  HBITMAP hbmScreenshot;
  CALL_HANDLE_WINAPI(hbmScreenshot, CreateCompatibleBitmap,
    hdcScreen,
    m_width,
    m_height);
  GDIObjectDeleter hbmScreenshot_deleter(hbmScreenshot);

  // Scope to bound when the bitmap is selected in the memory DC.
  {
    // Select the bitmap into the memory DC so that writing to the DC
    // writes to the bitmap.
    SELECT_RESTORE_OBJECT(memDC.m_hdc, hbmScreenshot);

    // Screenshot with result going to the memory DC.
    CALL_BOOL_WINAPI(BitBlt,
      memDC.m_hdc,                       // hdcDest
      0, 0,                              // xDest, yDest
      m_width,                           // wDest
      m_height,                          // hDest
      hdcScreen,                         // hdcSrc
      0, 0,                              // xSrc, ySrc
      SRCCOPY);                          // rop
  }

  // Now that everything has succeeded, acquire ownership of the
  // bitmap.
  m_bitmap = (HBITMAP)hbmScreenshot_deleter.release();
}


Screenshot::~Screenshot()
{
  if (m_bitmap) {
    CALL_BOOL_WINAPI(DeleteObject, m_bitmap);
  }
}


void Screenshot::drawToDC(HDC hdc, int x, int y, int w, int h)
{
  // Ignore zero-size draw requests.  This also avoids dividing by zero
  // when computing the aspect ratio if `h` is zero.
  if (w <= 0 || h <= 0) {
    return;
  }

  if (m_width <= 0 || m_height <= 0) {
    // If the screenshot is empty, just clear the entire rectangle.
    fillRectBG(hdc, x, y, w, h);
    return;
  }

  CompatibleHDC memDC(hdc);

  // Select the screenshot into the memory DC so the bitmap will act
  // as its data source.
  SELECT_RESTORE_OBJECT(memDC.m_hdc, m_bitmap);

  // The MS docs claim this is the "best" stretch mode.
  SetStretchBltMode(memDC.m_hdc, HALFTONE);

  // Aspect ratio of the source image.
  float srcAR = (float)m_width / (float)m_height;

  // Aspect ratio of the destination rectangle.
  float destAR = (float)w / (float)h;

  if (srcAR < destAR) {
    // Source is narrower, so draw bars on left and right.
    int properWidth = (int)(h * srcAR);
    int excess = w - properWidth;
    int leftBarW = excess / 2;
    int rightBarW = excess - leftBarW;

    // Left bar.
    fillRectBG(hdc, x, y, leftBarW, h);

    // Right bar.
    fillRectBG(hdc, x+leftBarW+properWidth, y, rightBarW, h);

    // Image.
    CALL_BOOL_WINAPI(StretchBlt,
      hdc, x+leftBarW, y, properWidth, h,        // dest, x, y, w, h
      memDC.m_hdc, 0, 0, m_width, m_height,      // src, x, y, w, h
      SRCCOPY);                                  // rop
  }

  else if (srcAR > destAR) {
    // Source is wider, so draw bars on top and bottom.
    int properHeight = (int)(w / srcAR);
    int excess = h - properHeight;
    int topBarH = excess / 2;
    int bottomBarH = excess - topBarH;

    // Top bar.
    fillRectBG(hdc, x, y, w, topBarH);

    // Bottom bar.
    fillRectBG(hdc, x, y+topBarH+properHeight, w, bottomBarH);

    // Image.
    CALL_BOOL_WINAPI(StretchBlt,
      hdc, x, y+topBarH, w, properHeight,        // dest, x, y, w, h
      memDC.m_hdc, 0, 0, m_width, m_height,      // src, x, y, w, h
      SRCCOPY);                                  // rop
  }

  else {
    // Matching aspect ratios, no need for bars.
    CALL_BOOL_WINAPI(StretchBlt,
      hdc, x, y, w, h,                           // dest, x, y, w, h
      memDC.m_hdc, 0, 0, m_width, m_height,      // src, x, y, w, h
      SRCCOPY);                                  // rop
  }
}


// EOF
