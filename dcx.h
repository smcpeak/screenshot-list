// dcx.h
// Class `DCX`, an HDC with some extensions.

#ifndef DCX_H
#define DCX_H

#include <string>                      // std::wstring
#include <vector>                      // std::vector

#include <windows.h>                   // HDC


// Display context augmented with a cursor area of sorts.
//
// The intention is to allow drawing functions to be composed by
// accepting a `DCX` object and thus restricting themselves
// (voluntarily) to the indicated part of the window being drawn.
//
// This approach is in contrast to the Windows "heavy weight" window
// approach, where a separate API-managed window object is createad for
// each distinct region.  That method requires a substantial amount of
// boilerplate code for each identifiable region and sub-region,
// discouraging factoring the drawing code in a compositional, reusable
// way.
//
class DCX {
public:      // data
  // Whereas my usual naming convention is to use the `m_` prefix on all
  // class data members, in this case I think it somewhat obscures the
  // coordinate arithmetic, so in this class, that prefix is not used.

  // The display context.  This class does *not* own the DC and will not
  // destroy or release it on exit.
  HDC hdc;

  // A "cursor", as a region within the display context that we are
  // intending to operate on.  This does not necessarily correspond to
  // any clipping region.  Rather, the idea is to encapsulate the area
  // where painting is intended to be focused.  Clients can freely
  // manipulate these fields to adjust the area of interest.
  //
  // This class, itself, does not impose any invariants on the
  // coordinate values.  In particular, they might be zero or negative.
  // Clients are expected to negotiate among themselves to deal with
  // those possibilities, generally by ignoring drawing requests that
  // are nonsensical when they arrive (especially non-positive sizes).
  // The drawing routines this class provides do that (albeit by letting
  // the underlying API do it).  That way, the arithmetic remains fairly
  // uniform and degenerate cases are handled with a minimum of
  // special-case logic.
  //
  int x;
  int y;
  int w;
  int h;

public:      // methods
  // Initialize to (0,0) of `hdc` using the given window dimensions.
  DCX(HDC hdc, HWND hwnd);

  // Make a copy.  DCX objects can be freely copied without side
  // effects.
  DCX(DCX const &obj) = default;
  DCX &operator=(DCX const &obj) = default;
  DCX &operator=(DCX &&obj) = default;

  // The destructor does nothing.
  ~DCX() = default;

  // Fill the area with one of the COLOR_XXX constants.
  void fillRectSysColor(int color) const;

  // Fill the area with the default window background color.
  void fillRectBG() const;

  // Draw `text` at (x,y) and return the size of the drawn text.
  SIZE textOut(std::wstring const &text) const;

  // Draw `text` and increment `m_y` by the text height.
  void textOut_incY(std::wstring const &text);

  // Given N widths, return a DCX that splits `*this` into N+1 columns,
  // where the leftmost column's width is whatever is left over after
  // the others are accounted for.  The resulting width might be
  // negative.  None of the input widths should be negative since that
  // would result in overlapping partitions.
  std::vector<DCX> splitHorizontallyFromRight(
    std::vector<int> const &widths) const;

  // Reduce the area by `margin` pixels on all four sides.
  void shrinkByMargin(int margin);
};


#endif // DCX_H
