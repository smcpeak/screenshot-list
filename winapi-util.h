// winapi-util.h
// Various winapi utilities.

// See license.txt for copyright and terms of use.

#ifndef WINAPI_UTIL_H
#define WINAPI_UTIL_H

#include <windows.h>                   // winapi

#include <string>                      // std::{string, wstring}


// Concatenate tokens.  Unlike plain '##', this works for __LINE__.  It
// is the same as BOOST_PP_CAT.
#define SMBASE_PP_CAT(a,b) SMBASE_PP_CAT2(a,b)
#define SMBASE_PP_CAT2(a,b) a##b


// put at the top of a class for which the default copy ctor
// and operator= are not desired; then don't define these functions
#define NO_OBJECT_COPIES(name)   \
  private:                       \
    name(name&);                 \
    void operator=(name&) /*user ;*/


// Stringize an argument as a wide string.
#define WIDE_STRINGIZE_HELPER(x) L ## x
#define WIDE_STRINGIZE(x) WIDE_STRINGIZE_HELPER(#x)


// Get the string corresponding to `errorCode`.  This string is a
// complete sentence, and does *not* end with a newline.
std::wstring getErrorMessage(DWORD errorCode);


// Get the string corresponding to `GetLastError()`.
std::wstring getLastErrorMessage();


// Get the string corresponding to `hr`.
std::wstring getHRErrorMessage(HRESULT hr);


// Given that `functionName` has failed, print an error message based on
// `GetLastError()` to stderr and exit(2).
void winapiDie(wchar_t const *functionName);


// Call `function`, which returns a handle on success and null on
// failure, with the specified arguments.  Store the result in `hvar`.
// Die if it fails.
#define CALL_HANDLE_WINAPI(hvar, function, ...) \
  if (!(hvar = function(__VA_ARGS__))) {        \
    winapiDie(WIDE_STRINGIZE(function));        \
  }


// Call `function`, which returns a `BOOL` indicating success, with the
// specified arguments.  Die if it fails, printing `GetLastError()`.
#define CALL_BOOL_WINAPI(function, ...)  \
  if (!function(__VA_ARGS__)) {          \
    winapiDie(WIDE_STRINGIZE(function)); \
  }


// Call `function`, which returns a `BOOL` indicating success, with the
// specified arguments.  Die if it fails, printing no reason because the
// API does not provide one.
#define CALL_BOOL_WINAPI_NLE(function, ...) \
  if (!function(__VA_ARGS__)) {             \
    winapiDieNLE(WIDE_STRINGIZE(function)); \
  }


// Given that `functionName` has failed, but that function does not set
// `GetLastError()` ("NLE" stands for "No Last Error"), print an error
// message to stderr and exit(2).
void winapiDieNLE(wchar_t const *functionName);


// `functionName` failed with code `hr`.  Print an error and exit(2).
void winapiDieHR(wchar_t const *functionName, HRESULT hr);


// Call `function`, which returns an `HRESULT`, with the specified
// arguments.  Die if it fails.
#define CALL_HR_WINAPI(function, ...)                   \
  if (HRESULT hr = function(__VA_ARGS__); FAILED(hr)) { \
    winapiDieHR(WIDE_STRINGIZE(function), hr);          \
  }


// Convert from narrow string, assumed to use UTF-8 encoding, to wide
// string.
std::wstring toWideString(std::string const &str);


// Structure to hold the arguments for a `CreateWindowExW` call.
class CreateWindowExWArgs {
public:      // data
  // Extended window style.  Initially 0.
  DWORD     m_dwExStyle;

  // Name of the window class.  Initially null.
  LPCWSTR   m_lpClassName;

  // Window text, used as the title for top-level window, text for
  // buttons, etc.  Initially null.
  LPCWSTR   m_lpWindowName;

  // Window style.  Initially 0.
  DWORD     m_dwStyle;

  // Initial window position and size.  Initially `CW_USEDEFAULT`.
  int       m_x;
  int       m_y;
  int       m_nWidth;
  int       m_nHeight;

  // Parent window.  Initially null.
  HWND      m_hwndParent;

  // Menu.  Initially null.
  HMENU     m_hMenu;

  // Instance handle.  Initially `GetModuleHandle(nullptr)`.
  HINSTANCE m_hInstance;

  // User data.  Initially null.
  LPVOID    m_lpParam;

public:      // methods
  CreateWindowExWArgs();

  // Pass the arguments to `CreateWindowExW`, returning whatever it
  // returns.
  HWND createWindow() const;
};


// If `ptr` is not null, call its `Release` method and nullify it.
template <typename T>
void safeRelease(T *&ptr)
{
  if (ptr) {
    ptr->Release();
    ptr = nullptr;
  }
}


// Class that calls `SelectObject` in its ctor, then again in its dtor
// to restore the value.
//
// Note: This cannot be used to select region objects!  The
// `SelectObject` API for them is different.
//
class SelectRestoreObject {
  NO_OBJECT_COPIES(SelectRestoreObject);

public:      // data
  // The display context being manipulated.
  HDC m_hdc;

  // The previous object of the same type as that passed to the ctor.
  HGDIOBJ m_prevObj;

public:      // methods
  SelectRestoreObject(HDC hdc, HGDIOBJ newObj);
  ~SelectRestoreObject();
};


// Use `SelectObject` to select `hobj` within `hdc`, then upon exiting
// the scope, restore the previous object that had that type.
#define SELECT_RESTORE_OBJECT(hdc, hobj) \
  SelectRestoreObject SMBASE_PP_CAT(selRestorer,__LINE__)((hdc), (hobj)) /* user ; */


// Hold a handle to a display context (HDC), releasing it with
// `ReleaseDC` when exiting scope.
class HDCReleaser {
  NO_OBJECT_COPIES(HDCReleaser);

public:      // data
  // The window with which the DC is associated.  (It is dumb that this
  // is needed, but that is how `ReleaseDC` works.)
  HWND m_hwnd;

  // The HDC we are holding.
  HDC m_hdc;

public:      // methods
  HDCReleaser(HWND hwnd, HDC hdc);
  ~HDCReleaser();
};


// Declare an HDC called `hdcVar`.  Initialize it by calling
// `GetDC(hwnd)`.  At the end of the scope, release it.
#define GET_AND_RELEASE_HDC(hdcVar, hwnd)      \
  HDC hdcVar;                                  \
  CALL_HANDLE_WINAPI(hdcVar, GetDC, hwnd);     \
  HDCReleaser hdcVar##_releaser(hwnd, hdcVar);


// Create and destroy an HDC compatible with another.
class CompatibleHDC {
public:      // data
  // The new HDC.
  HDC m_hdc;

public:      // methods
  CompatibleHDC(HDC other);
  ~CompatibleHDC();

  HDC getDC() const { return m_hdc; }
};


// Display context and backing bitmap for use as a hidden drawing
// surface.
class BitmapDC {
public:      // data
  // The display context.
  CompatibleHDC m_memDC;

  // The backing bitmap, created by the constructor.  This will be
  // destroyed when this object is if it is not null.
  HBITMAP m_bitmap;

private:     // data
  // Cause the bitmap to be selected into the DC, and remove it at the
  // end.
  SelectRestoreObject m_selectRestore;

public:      // methods
  // Create the DC and bitmap to be compatible with `hdc`.
  BitmapDC(HDC hdc, int w, int h);

  // Destroy the memory DC and, if not null, the bitmap.
  ~BitmapDC();

  HDC getDC() const { return m_memDC.getDC(); }

  // Return the bitmap and nullify `m_bitmap`.  This way, one can use
  // this object to populate the bitmap and keep it for later use,
  // discarding the DC in the meantime.
  HBITMAP releaseBitmap();
};


// Delete a GDI object when going out of scope.
class GDIObjectDeleter {
  NO_OBJECT_COPIES(GDIObjectDeleter);

public:      // data
  // The object to be deleted.  This can be set to null to do nothing in
  // the dtor.
  HGDIOBJ m_obj;

public:      // methods
  GDIObjectDeleter(HGDIOBJ obj);
  ~GDIObjectDeleter();

  // Return the handle and nullify `m_obj`.
  HGDIOBJ release();
};


// Paint a rectangle to `hdc` using the window background color.
void fillRectBG(HDC hdc, int x, int y, int w, int h);

// Fill with one of the COLOR_XXX constants.
void fillRectSysColor(HDC hdc, int x, int y, int w, int h, int color);

// Draw `text` at the given coordinate.  Return its pixel dimensions.
SIZE textOut(HDC hdc, int x, int y, std::wstring const &text);

// Create a bitmap compatible with `hdc`, doing its own error checking.
HBITMAP createCompatibleBitmap(HDC hdc, int w, int h);

// Get the client area.  Usually (always?) the top-left is (0,0).
RECT getWindowClientArea(HWND hwnd);

// Get the window client region height in pixels.
int getWindowClientHeight(HWND hwnd);


#endif // WINAPI_UTIL_H
