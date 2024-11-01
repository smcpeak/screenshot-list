// screenshot.cc
// Code for `screenshot.h`.

#include "screenshot.h"                // this module

#include "winapi-util.h"               // CompatibleDC, etc.

#include <cmath>                       // std::ceil
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

  // Screenshot with result going to a memory DC.
  BitmapDC memDC(hdcScreen, m_width, m_height);
  CALL_BOOL_WINAPI(BitBlt,
    memDC.getDC(),                     // hdcDest
    0, 0,                              // xDest, yDest
    m_width,                           // wDest
    m_height,                          // hDest
    hdcScreen,                         // hdcSrc
    0, 0,                              // xSrc, ySrc
    SRCCOPY);                          // rop

  // Take ownership of the bitmap.
  m_bitmap = memDC.releaseBitmap();
}


Screenshot::~Screenshot()
{
  if (m_bitmap) {
    CALL_BOOL_WINAPI(DeleteObject, m_bitmap);
  }
}


void Screenshot::drawToDC(HDC hdc, int x, int y, int w, int h) const
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


int Screenshot::drawToDC_autoHeight(HDC hdc, int x, int y, int w) const
{
  int h = heightForWidth(w);
  drawToDC(hdc, x, y, w, h);
  return h;
}


int Screenshot::drawToDCX_autoHeight(DCX const &dcx) const
{
  return drawToDC_autoHeight(dcx.hdc, dcx.x, dcx.y, dcx.w);
}


int Screenshot::heightForWidth(int w) const
{
  if (m_width > 0) {
    return (int)std::ceil((float)m_height * (float)w / (float)m_width);
  }
  else {
    return 0;
  }
}


// EOF
