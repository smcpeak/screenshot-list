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


void DCX::textOut_moveTop(std::wstring const &text)
{
  moveTopBy(this->textOut(text).cy);
}


// Return the sum of all elements in `vec`.
static int vectorSum(std::vector<int> const &vec)
{
  int ret = 0;

  for (int n : vec) {
    ret += n;
  }

  return ret;
}


std::vector<DCX> DCX::splitHorizontallyFromRight(
  std::vector<int> const &widths) const
{
  std::vector<DCX> ret;

  // Start with the left partition taking all space remaining after
  // the other columns are accounted for.
  DCX dcx(*this);
  dcx.w -= vectorSum(widths);
  ret.push_back(dcx);

  for (int w : widths) {
    // Construct the next partition.
    dcx.x += dcx.w;
    dcx.w = w;
    ret.push_back(dcx);
  }

  return ret;
}


void DCX::shrinkByMargin(int margin)
{
  x += margin;
  y += margin;
  w -= margin*2;
  h -= margin*2;
}


void DCX::moveTopBy(int dy)
{
  y += dy;
  h -= dy;
}


// EOF
