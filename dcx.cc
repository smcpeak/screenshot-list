// dcx.cc
// Code for `dcx.h`.

#include "dcx.h"                       // this module

#include "winapi-util.h"               // fillRectSysColor, etc.


DCX::DCX(HDC hdc, HWND hwnd)
  : hdc(hdc),
    x(0),
    y(0),
    w(0),
    h(0)
{
  RECT rcClient;
  CALL_BOOL_WINAPI(GetClientRect, hwnd, &rcClient);

  this->w = rcClient.right;
  this->h = rcClient.bottom;
}


void DCX::fillRectSysColor(int color) const
{
  ::fillRectSysColor(hdc, x, y, w, h, color);
}


void DCX::fillRectBG() const
{
  ::fillRectBG(hdc, x, y, w, h);
}


SIZE DCX::textOut(std::wstring const &text) const
{
  return ::textOut(hdc, x, y, text);
}


void DCX::textOut_incY(std::wstring const &text)
{
  this->y += this->textOut(text).cy;
}


// EOF
