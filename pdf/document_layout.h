// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_DOCUMENT_LAYOUT_H_
#define PDF_DOCUMENT_LAYOUT_H_

#include <vector>

#include "pdf/draw_utils/coordinates.h"
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

    // Returns the default page orientation, encoded as an integer from 0 to 3
    // (inclusive).
    //
    // A return value of 0 indicates the original page orientation, with each
    // increment indicating clockwise rotation by an additional 90 degrees.
    //
    // TODO(kmoon): Return an enum (class) instead of an integer.
    int default_page_orientation() const { return default_page_orientation_; }

    // Rotates default page orientation 90 degrees clockwise.
    void RotatePagesClockwise();

    // Rotates default page orientation 90 degrees counterclockwise.
    void RotatePagesCounterclockwise();

   private:
    // Orientations are non-negative integers modulo 4.
    int default_page_orientation_ = 0;
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

  // Sets the layout's total size.
  void set_size(const pp::Size& size) { size_ = size; }

  // Given |page_sizes| and the layout's width set to the max width of the
  // document's pages, return pp::Rects that represent |page_sizes|
  // formatted for single view and update the layout's size to the size of the
  // new single view layout.
  std::vector<pp::Rect> GetSingleViewLayout(
      const std::vector<pp::Size>& page_sizes);

  // Given |page_sizes| and the layout's width is set to the max page width of
  // the document's pages, return pp::Rects that represent |page_sizes|
  // formatted for two-up view and update the layout's size to the size of the
  // new two-up view layout.
  std::vector<pp::Rect> GetTwoUpViewLayout(
      const std::vector<pp::Size>& page_sizes);

  // Increases the layout's total height by |height|.
  void EnlargeHeight(int height);

  // Appends a rectangle of size |page_rect| to the layout. This will increase
  // the layout's height by the page's height, and increase the layout's width
  // to at least the page's width.
  void AppendPageRect(const pp::Size& page_rect);

 private:
  Options options_;

  // Layout's total size.
  pp::Size size_;
};

}  // namespace chrome_pdf

#endif  // PDF_DOCUMENT_LAYOUT_H_
