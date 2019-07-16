// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_DRAW_UTILS_COORDINATES_H_
#define PDF_DRAW_UTILS_COORDINATES_H_

#include "ppapi/cpp/rect.h"

namespace pp {
class Point;
}

namespace chrome_pdf {
namespace draw_utils {

// Struct for sending the sizes of the insets applied to the page to accommodate
// for shadows/separators. I.e. the left component corresponds to the amount a
// page was inset on the left side.
struct PageInsetSizes {
  int left;
  int right;
  int top;
  int bottom;
};

// Given |rect_size|, sets the width of |doc_size| to the max of |rect_size|'s
// width and |doc_size|'s width. Also adds the height of |rect_size| to
// |doc_size|'s height.
void ExpandDocumentSize(const pp::Size& rect_size, pp::Size* doc_size);

// Given |rect| in document coordinates, a |position| in screen coordinates, and
// a |zoom| factor, returns the rectangle in screen coordinates (i.e. 0,0 is top
// left corner of plugin area).
// An empty |rect| will always result in an empty output rect.
// For |zoom|, a value of 1 means 100%. |zoom| is never less than or equal to 0.
pp::Rect GetScreenRect(const pp::Rect& rect,
                       const pp::Point& position,
                       double zoom);

// Given |rect_size|, create a pp::Rect where the top-right corner lies at
// |position|, and then inset it with the corresponding values of |page_insets|,
// i.e. inset on left side with |page_insets.left|.
// The width of |rect_size| must be less than or equal to the x value for
// |position|.
pp::Rect GetLeftRectForTwoUpView(const pp::Size& rect_size,
                                 const pp::Point& position,
                                 const PageInsetSizes& page_insets);

// Given |rect_size|, create a pp::Rect where the top-left corner lies at
// |position|, and then inset it with the corresponding values of |page_insets|,
// i.e. inset on left side with |page_insets.left|.
pp::Rect GetRightRectForTwoUpView(const pp::Size& rect_size,
                                  const pp::Point& position,
                                  const PageInsetSizes& page_insets);

}  // namespace draw_utils
}  // namespace chrome_pdf

#endif  // PDF_DRAW_UTILS_COORDINATES_H_
