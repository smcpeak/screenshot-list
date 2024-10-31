// screenshot.cc
// Code for `screenshot.h`.

#include "screenshot.h"                // this module

#include <cstdio>                      // std::snprintf
#include <ctime>                       // std::{time_t, tm, localtime, time}


// Return the current date/time, in the local time zone, in
// "YYYY-MM-DD hh:mm" format.
static std::string getLocaltime()
{
  std::time_t time = std::time(nullptr);
  std::tm const *t = std::localtime(&time);

  // 17 would be enough for 4-digit years, but years do not technically
  // have an upper bound.
  char buf[30];

  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d",
    t->tm_year + 1900,
    t->tm_mon + 1,
    t->tm_mday,
    t->tm_hour,
    t->tm_min);

  return std::string(buf);
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


// EOF
