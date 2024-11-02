// screenshot.cc
// Code for `screenshot.h`.

#include "screenshot.h"                // this module

#include "trace.h"                     // TRACE2
#include "winapi-util.h"               // CompatibleDC, etc.

#include <cassert>                     // assert
#include <cmath>                       // std::ceil
#include <cwchar>                      // std::swprintf
#include <string>                      // std::wstring

#include <windows.h>                   // GetLocalTime, etc.


Screenshot::Screenshot()
  : m_bitmap(nullptr),
    m_width(GetSystemMetrics(SM_CXSCREEN)),
    m_height(GetSystemMetrics(SM_CYSCREEN)),
    m_fname()
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

  // Chose an unused file name.
  chooseFileName();

  // Create any directories needed for the name.
  createParentDirectoriesOf(m_fname);

  // Save the image to the chosen name.
  writeToBMPFile(m_fname);
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

  // Change the awful default B+W stretching mode to something that
  // works properly with color images.
  SetStretchBltMode(hdc, HALFTONE);

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


void Screenshot::chooseFileName()
{
  // Choose the name based on the current time.
  SYSTEMTIME st;
  GetLocalTime(&st);

  // Disambiguation loop.
  for (int suffixNumber = 1; suffixNumber < 100; ++suffixNumber) {
    // Normally there is no suffix.
    wchar_t suffix[10] = L"";

    // But add "s2", "s3", etc. when needed.  The "s" stands for "shot".
    if (suffixNumber > 1) {
      swprintf(suffix, TABLESIZE(suffix), L"s%02d", suffixNumber);
      TRACE2(L"suffix: " << suffix);
    }

    wchar_t buf[80];

    swprintf(buf, TABLESIZE(buf),
      L"shots/%04d-%02d-%02dT%02d-%02d-%02d%ls.bmp",
      (int)st.wYear,
      (int)st.wMonth,
      (int)st.wDay,
      (int)st.wHour,
      (int)st.wMinute,
      (int)st.wSecond,
      suffix);
    TRACE2(L"buf: " << buf);

    if (!pathExists(buf)) {
      m_fname = std::wstring(buf);
      return;
    }
  }

  die(L"Screenshot::chooseFileName: failed to pick a unique file name");
}



// Based in part on
// https://learn.microsoft.com/en-us/windows/win32/gdi/capturing-an-image
void Screenshot::writeToBMPFile(std::wstring const &fname) const
{
  assert(m_bitmap);

  // Get image dimensions, etc.
  BITMAP bmp{};
  CALL_BOOL_WINAPI_NLE(GetObject, m_bitmap, sizeof(bmp), &bmp);

  // Prepare the second part of the header.
  BITMAPINFOHEADER biHeader{};
  biHeader.biSize = sizeof(biHeader);
  biHeader.biWidth = bmp.bmWidth;
  biHeader.biHeight = bmp.bmHeight;
  biHeader.biPlanes = 1;
  biHeader.biBitCount = 32;
  biHeader.biCompression = BI_RGB;
  biHeader.biSizeImage = 0;
  biHeader.biXPelsPerMeter = 0;
  biHeader.biYPelsPerMeter = 0;
  biHeader.biClrUsed = 0;
  biHeader.biClrImportant = 0;

  // Total size in bytes of the pixel data.
  std::size_t pixelDataSizeBytes =
    ((bmp.bmWidth * biHeader.biBitCount + 31) / 32) * 4 * bmp.bmHeight;

  // Allocate memory to store it.
  std::vector<char> pixelData(pixelDataSizeBytes, 0);

  // Extract the image data from the GDI object.
  {
    // The GDI object was originally created using the screen as a
    // source, so we need another screen DC to decode it.
    GET_AND_RELEASE_HDC(hdcScreen, NULL);

    // The documentation nonsensically says this function can "return"
    // `ERROR_INVALID_PARAMETER`.  How?  It does not say it sets
    // `GetLastError()`, and in my experience, if the function does not
    // say it sets GLE then it does not.  And anyway there is evidently
    // only one possible error code it can "return", which means it
    // conveys no information.  So I treat this function as not being
    // able to return any error information.
    CALL_BOOL_WINAPI_NLE(GetDIBits,
      hdcScreen,             // hdc: DC with which the GDI object is compatible.
      m_bitmap,              // hbm: Input GDI object.
      0,                     // start: First scan (sic) line.
      (UINT)bmp.bmHeight,    // cLines: Number of lines.
      pixelData.data(),      // lpvBits: Array to write to.
      (BITMAPINFO*)&biHeader,// lpbmi: Desired output format.
      DIB_RGB_COLORS);       // usage: Whether to use a palette (here, no).
  }

  // Prepare the first part of the header.
  BITMAPFILEHEADER bmfHeader{};
  bmfHeader.bfType = 0x4D42;           // "BM", in little-endian.
  bmfHeader.bfSize =                   // Total file size in bytes.
    sizeof(bmfHeader) +                  // header 1
    sizeof(biHeader) +                   // header 2
    pixelDataSizeBytes;                  // pixel data
  bmfHeader.bfReserved1 = 0;
  bmfHeader.bfReserved2 = 0;
  bmfHeader.bfOffBits =                // Offset to pixel data.
    sizeof(bmfHeader) +                  // header 1
    sizeof(biHeader);                    // header 2

  // Create the file.
  HANDLE hFile;
  CALL_HANDLE_WINAPI(hFile, CreateFileW,
    fname.c_str(),                     // lpFileName
    GENERIC_WRITE,                     // dwDesiredAccess
    0,                                 // dwShareMode
    NULL,                              // lpSecurityAttributes
    CREATE_ALWAYS,                     // dwCreationDisposition
    FILE_ATTRIBUTE_NORMAL,             // dwFlagsAndAttributes
    NULL);                             // hTemplateFile
  HandleCloser hFile_closer(hFile);

  // Write the image data.
  DWORD dwBytesWritten = 0;            // ignored
  CALL_BOOL_WINAPI(WriteFile, hFile,
    &bmfHeader, sizeof(bmfHeader), &dwBytesWritten, NULL);
  CALL_BOOL_WINAPI(WriteFile, hFile,
    &biHeader, sizeof(biHeader), &dwBytesWritten, NULL);
  CALL_BOOL_WINAPI(WriteFile, hFile,
    pixelData.data(), pixelData.size(), &dwBytesWritten, NULL);

  hFile_closer.close();
}


// EOF
