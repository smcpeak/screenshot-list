// screenshot.cc
// Code for `screenshot.h`.

#include "screenshot.h"                // this module

#include <cwchar>                      // std::swprintf
#include <string>                      // std::wstring


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


// EOF
