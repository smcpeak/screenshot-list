// winapi-util.cc
// Code for `winapi-util` module.

// See license.txt for copyright and terms of use.

#include "winapi-util.h"               // this module

#include <windows.h>                   // winapi

#include <codecvt>                     // std::wstring_convert
#include <cstdlib>                     // std::exit
#include <exception>                   // std::exception
#include <iostream>                    // std::{wcerr, clog, endl}
#include <locale>                      // std::wstring_convert
#include <sstream>                     // std::wostreamstream
#include <string>                      // std::wstring


// This is copied+modified from smbase/syserr.cc.
std::wstring getErrorMessage(DWORD errorCode)
{
  // Get the string for `errorCode`.
  LPVOID lpMsgBuf;
  if (!FormatMessage(
         FORMAT_MESSAGE_ALLOCATE_BUFFER |
         FORMAT_MESSAGE_FROM_SYSTEM |
         FORMAT_MESSAGE_IGNORE_INSERTS,
         NULL,
         errorCode,
         MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
         (LPTSTR) &lpMsgBuf,
         0,
         NULL)) {
    std::wcerr << L"FormatMessage failed with code "
               << GetLastError() << L"\n";
    std::exit(4);
  }

  // Now the SDK says: "Process any inserts in lpMsgBuf."
  //
  // I think this means that lpMsgBuf might have "%" escape
  // sequences in it... oh well, I'm just going to keep them.

  // Make a copy of the string.
  std::wstring ret((wchar_t*)lpMsgBuf);

  // Free the buffer.
  if (LocalFree(lpMsgBuf) != NULL) {
    winapiDie(L"LocalFree");
  }

  // At least some error messages end with a newline, but I do not want
  // that.
  if (!ret.empty() && ret[ret.size()-1] == L'\n') {
    ret = ret.substr(0, ret.size()-1);
  }

  return ret;
}


std::wstring getLastErrorMessage()
{
  return getErrorMessage(GetLastError());
}


std::wstring getHRErrorMessage(HRESULT hr)
{
  DWORD facility = HRESULT_FACILITY(hr);
  DWORD code = HRESULT_CODE(hr);

  if (facility == FACILITY_WIN32) {
    return getErrorMessage(code);
  }
  else {
    std::wostringstream oss;
    oss << std::hex;
    oss << L"HRESULT error: facility=" << facility
        << L" code=" << code << L".";
    return oss.str();
  }
}


void winapiDie(wchar_t const *functionName)
{
  DWORD code = GetLastError();
  std::wcerr << functionName << L": " << getErrorMessage(code) << L"\n";
  std::exit(2);
}


void winapiDieNLE(wchar_t const *functionName)
{
  std::wcerr << functionName << L" failed.\n";
  std::exit(2);
}


void winapiDieHR(wchar_t const *functionName, HRESULT hr)
{
  std::wcerr << functionName << L": " << getHRErrorMessage(hr) << L"\n";
  std::exit(2);
}


// https://stackoverflow.com/questions/2573834/c-convert-string-or-char-to-wstring-or-wchar-t
std::wstring toWideString(std::string const &str)
{
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
  return converter.from_bytes(str);
}


// ----------------------- CreateWindowExWArgs -------------------------
CreateWindowExWArgs::CreateWindowExWArgs()
  : m_dwExStyle(0),
    m_lpClassName(nullptr),
    m_lpWindowName(nullptr),
    m_dwStyle(0),
    m_x(CW_USEDEFAULT),
    m_y(CW_USEDEFAULT),
    m_nWidth(CW_USEDEFAULT),
    m_nHeight(CW_USEDEFAULT),
    m_hwndParent(nullptr),
    m_hMenu(nullptr),
    m_hInstance(GetModuleHandle(nullptr)),
    m_lpParam(nullptr)
{}


HWND CreateWindowExWArgs::createWindow() const
{
  return CreateWindowExW(
    m_dwExStyle,
    m_lpClassName,
    m_lpWindowName,
    m_dwStyle,
    m_x,
    m_y,
    m_nWidth,
    m_nHeight,
    m_hwndParent,
    m_hMenu,
    m_hInstance,
    m_lpParam);
}


// ------------------------ SelectRestoreObject ------------------------
SelectRestoreObject::SelectRestoreObject(HDC hdc, HGDIOBJ newObj)
  : m_hdc(hdc),
    m_prevObj(nullptr)
{
  m_prevObj = SelectObject(m_hdc, newObj);
  if (!m_prevObj) {
    winapiDieNLE(L"SelectObject(set)");
  }
}


SelectRestoreObject::~SelectRestoreObject()
{
  // Restore the old object.
  if (!SelectObject(m_hdc, m_prevObj)) {
    winapiDieNLE(L"SelectObject(restore)");
  }
}


// ---------------------------- HDCReleaser ----------------------------
HDCReleaser::HDCReleaser(HWND hwnd, HDC hdc)
  : m_hwnd(hwnd),
    m_hdc(hdc)
{}


HDCReleaser::~HDCReleaser()
{
  CALL_BOOL_WINAPI(ReleaseDC, m_hwnd, m_hdc);
}


// --------------------------- HandleCloser ----------------------------
HandleCloser::HandleCloser(HANDLE handle)
  : m_handle(handle)
{}


HandleCloser::~HandleCloser() noexcept
{
  try {
    close();
  }
  catch (std::exception &e) {
    std::clog << e.what() << std::endl;
  }
}


void HandleCloser::close()
{
  if (m_handle) {
    CALL_BOOL_WINAPI(CloseHandle, m_handle);
    m_handle = nullptr;
  }
}


// --------------------------- CompatibleHDC ---------------------------
CompatibleHDC::CompatibleHDC(HDC other)
  : m_hdc(nullptr)
{
  CALL_HANDLE_WINAPI(m_hdc, CreateCompatibleDC, other);
}


CompatibleHDC::~CompatibleHDC()
{
  if (m_hdc) {
    DeleteDC(m_hdc);
  }
}


// ----------------------------- BitmapDC ------------------------------
BitmapDC::BitmapDC(HDC hdc, int w, int h)
  : m_memDC(hdc),
    m_bitmap(createCompatibleBitmap(hdc, w, h)),
    m_selectRestore(m_memDC.m_hdc, m_bitmap)
{}


BitmapDC::~BitmapDC()
{
  if (m_bitmap) {
    CALL_BOOL_WINAPI_NLE(DeleteObject, m_bitmap);
  }
}


HBITMAP BitmapDC::releaseBitmap()
{
  HBITMAP ret = m_bitmap;
  m_bitmap = nullptr;
  return ret;
}


// ------------------------- GDIObjectDeleter --------------------------
GDIObjectDeleter::GDIObjectDeleter(HGDIOBJ obj)
  : m_obj(obj)
{}


GDIObjectDeleter::~GDIObjectDeleter()
{
  if (m_obj) {
    CALL_BOOL_WINAPI(DeleteObject, m_obj);
  }
}


HGDIOBJ GDIObjectDeleter::release()
{
  HGDIOBJ ret = m_obj;
  m_obj = nullptr;
  return ret;
}


// ----------------------------- GDI utils -----------------------------
void fillRectBG(HDC hdc, int x, int y, int w, int h)
{
  fillRectSysColor(hdc, x, y, w, h, COLOR_WINDOW);
}


void fillRectSysColor(HDC hdc, int x, int y, int w, int h, int color)
{
  HBRUSH brush;
  CALL_HANDLE_WINAPI(brush, GetSysColorBrush, color);

  RECT r;
  r.left = x;
  r.top = y;
  r.right = x+w;
  r.bottom = y+h;

  FillRect(hdc, &r, brush);
}


SIZE textOut(HDC hdc, int x, int y, std::wstring const &text)
{
  CALL_BOOL_WINAPI(TextOut,
    hdc,
    x, y,
    text.data(),
    text.size());

  SIZE sz;
  CALL_BOOL_WINAPI(GetTextExtentPoint32W,
    hdc,
    text.data(),
    text.size(),
    &sz);

  return sz;
}


HBITMAP createCompatibleBitmap(HDC hdc, int w, int h)
{
  HBITMAP ret;
  CALL_HANDLE_WINAPI(ret, CreateCompatibleBitmap,
    hdc, w, h);
  return ret;
}


RECT getWindowClientArea(HWND hwnd)
{
  RECT rcClient;
  CALL_BOOL_WINAPI(GetClientRect, hwnd, &rcClient);
  return rcClient;
}


int getWindowClientHeight(HWND hwnd)
{
  RECT r = getWindowClientArea(hwnd);
  return r.bottom - r.top;
}


// ------------------------------- Menus -------------------------------
HMENU createMenu()
{
  HMENU ret;
  CALL_HANDLE_WINAPI(ret, CreateMenu);
  return ret;
}


void setMenu(HWND hwnd, HMENU menu)
{
  CALL_BOOL_WINAPI(SetMenu, hwnd, menu);
}


void appendMenuW(
  HMENU    hMenu,
  UINT     uFlags,
  UINT_PTR uIDNewItem,
  LPCWSTR  lpNewItem)
{
  CALL_BOOL_WINAPI(AppendMenuW, hMenu, uFlags, uIDNewItem, lpNewItem);
}


// ------------------------------- Files -------------------------------
void writeFile(HANDLE hFile, void const *data, std::size_t size)
{
  // Ignored.
  DWORD dwBytesWritten = 0;

  CALL_BOOL_WINAPI(WriteFile, hFile, data, size, &dwBytesWritten, NULL);
}


// EOF
