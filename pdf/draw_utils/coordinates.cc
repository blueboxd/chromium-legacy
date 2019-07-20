// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/draw_utils/coordinates.h"

#include <math.h>

#include "base/logging.h"
#include "ppapi/cpp/point.h"

namespace chrome_pdf {
namespace draw_utils {

void ExpandDocumentSize(const pp::Size& rect_size, pp::Size* doc_size) {
  int width_diff = std::max(0, rect_size.width() - doc_size->width());
  doc_size->Enlarge(width_diff, rect_size.height());
}

pp::Rect GetLeftFillRect(const pp::Rect& page_rect,
                         const PageInsetSizes& inset_sizes,
                         int bottom_separator) {
  DCHECK_GE(page_rect.x(), inset_sizes.left);

  return pp::Rect(0, page_rect.y() - inset_sizes.top,
                  page_rect.x() - inset_sizes.left,
                  page_rect.height() + inset_sizes.top + inset_sizes.bottom +
                      bottom_separator);
}

pp::Rect GetRightFillRect(const pp::Rect& page_rect,
                          const PageInsetSizes& inset_sizes,
                          int doc_width,
                          int bottom_separator) {
  int right_gap_x = page_rect.right() + inset_sizes.right;
  DCHECK_GE(doc_width, right_gap_x);

  return pp::Rect(right_gap_x, page_rect.y() - inset_sizes.top,
                  doc_width - right_gap_x,
                  page_rect.height() + inset_sizes.top + inset_sizes.bottom +
                      bottom_separator);
}

pp::Rect GetBottomFillRect(const pp::Rect& page_rect,
                           const PageInsetSizes& inset_sizes,
                           int bottom_separator) {
  return pp::Rect(page_rect.x() - inset_sizes.left,
                  page_rect.bottom() + inset_sizes.bottom,
                  page_rect.width() + inset_sizes.left + inset_sizes.right,
                  bottom_separator);
}

pp::Rect GetScreenRect(const pp::Rect& rect,
                       const pp::Point& position,
                       double zoom) {
  DCHECK_GT(zoom, 0);

  int x = static_cast<int>(rect.x() * zoom - position.x());
  int y = static_cast<int>(rect.y() * zoom - position.y());
  int right = static_cast<int>(ceil(rect.right() * zoom - position.x()));
  int bottom = static_cast<int>(ceil(rect.bottom() * zoom - position.y()));
  return pp::Rect(x, y, right - x, bottom - y);
}

pp::Rect GetSurroundingRect(const pp::Rect& rect,
                            const PageInsetSizes& inset_sizes,
                            int doc_width,
                            int bottom_separator) {
  return pp::Rect(
      0, rect.y() - inset_sizes.top, doc_width,
      rect.height() + inset_sizes.top + inset_sizes.bottom + bottom_separator);
}

pp::Rect GetLeftRectForTwoUpView(const pp::Size& rect_size,
                                 const pp::Point& position,
                                 const PageInsetSizes& page_insets) {
  DCHECK_LE(rect_size.width(), position.x());

  pp::Rect left_rect(position.x() - rect_size.width(), position.y(),
                     rect_size.width(), rect_size.height());
  left_rect.Inset(page_insets.left, page_insets.top, page_insets.right,
                  page_insets.bottom);
  return left_rect;
}

pp::Rect GetRightRectForTwoUpView(const pp::Size& rect_size,
                                  const pp::Point& position,
                                  const PageInsetSizes& page_insets) {
  pp::Rect right_rect(position.x(), position.y(), rect_size.width(),
                      rect_size.height());
  right_rect.Inset(page_insets.left, page_insets.top, page_insets.right,
                   page_insets.bottom);
  return right_rect;
}

}  // namespace draw_utils
}  // namespace chrome_pdf
