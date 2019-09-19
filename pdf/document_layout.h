// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_DOCUMENT_LAYOUT_H_
#define PDF_DOCUMENT_LAYOUT_H_

#include <cstddef>
#include <vector>

#include "base/logging.h"
#include "pdf/draw_utils/coordinates.h"
#include "pdf/page_orientation.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"

namespace chrome_pdf {

// Layout of pages within a PDF document. Pages are placed as rectangles
// (possibly rotated) in a non-overlapping vertical sequence.
//
// All layout units are pixels.
//
// The |Options| class controls the behavior of the layout, such as the default
// orientation of pages.
class DocumentLayout final {
 public:
  // Options controlling layout behavior.
  class Options final {
   public:
    Options();

    Options(const Options& other);
    Options& operator=(const Options& other);

    ~Options();

    PageOrientation default_page_orientation() const {
      return default_page_orientation_;
    }

    // Rotates default page orientation 90 degrees clockwise.
    void RotatePagesClockwise();

    // Rotates default page orientation 90 degrees counterclockwise.
    void RotatePagesCounterclockwise();

   private:
    PageOrientation default_page_orientation_ = PageOrientation::kOriginal;
  };

  static const draw_utils::PageInsetSizes kSingleViewInsets;
  static constexpr int32_t kBottomSeparator = 4;
  static constexpr int32_t kHorizontalSeparator = 1;

  DocumentLayout();

  DocumentLayout(const DocumentLayout& other) = delete;
  DocumentLayout& operator=(const DocumentLayout& other) = delete;

  ~DocumentLayout();

  // Returns the layout options.
  const Options& options() const { return options_; }

  // Sets the layout options.
  void set_options(const Options& options) { options_ = options; }

  // Returns the layout's total size.
  const pp::Size& size() const { return size_; }

  size_t page_count() const { return page_layouts_.size(); }

  // Gets the layout rectangle for a page. Only valid after computing a layout.
  const pp::Rect& page_rect(size_t page_index) const {
    DCHECK_LT(page_index, page_count());
    return page_layouts_[page_index].outer_rect;
  }

  // Gets the layout rectangle for a page's bounds (which excludes additional
  // regions like page shadows). Only valid after computing a layout.
  const pp::Rect& page_bounds_rect(size_t page_index) const {
    DCHECK_LT(page_index, page_count());
    return page_layouts_[page_index].inner_rect;
  }

  // Computes layout that represent |page_sizes| formatted for single view.
  //
  // TODO(kmoon): Control layout type using an option.
  void ComputeSingleViewLayout(const std::vector<pp::Size>& page_sizes);

  // Computes layout that represent |page_sizes| formatted for two-up view.
  //
  // TODO(kmoon): Control layout type using an option.
  void ComputeTwoUpViewLayout(const std::vector<pp::Size>& page_sizes);

 private:
  // Layout of a single page.
  struct PageLayout {
    // Bounding rectangle for the page with decorations.
    pp::Rect outer_rect;

    // Bounding rectangle for the page without decorations.
    pp::Rect inner_rect;
  };

  Options options_;

  // Layout's total size.
  pp::Size size_;

  std::vector<PageLayout> page_layouts_;
};

}  // namespace chrome_pdf

#endif  // PDF_DOCUMENT_LAYOUT_H_
