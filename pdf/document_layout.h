// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_DOCUMENT_LAYOUT_H_
#define PDF_DOCUMENT_LAYOUT_H_

#include "ppapi/cpp/size.h"

namespace chrome_pdf {

// Layout of pages within a PDF document. Pages are placed as rectangles
// (possibly rotated) in a non-overlapping vertical sequence.
//
// All layout units are pixels.
//
// TODO(crbug.com/51472): Support multiple columns.
class DocumentLayout final {
 public:
  DocumentLayout();

  DocumentLayout(const DocumentLayout& other);
  DocumentLayout& operator=(const DocumentLayout& other);

  ~DocumentLayout();

  // Returns an integer from 0 to 3 (inclusive), encoding the default
  // orientation of the document's pages.
  //
  // A return value of 0 indicates the original page orientation, with each
  // increment indicating clockwise rotation by an additional 90 degrees.
  //
  // TODO(kmoon): Return an enum (class) instead of an integer.
  int default_page_orientation() const { return default_page_orientation_; }

  // Rotates all pages 90 degrees clockwise. Does not recompute layout.
  void RotatePagesClockwise();

  // Rotates all pages 90 degrees counterclockwise. Does not recompute layout.
  void RotatePagesCounterclockwise();

  // Returns the layout's total size.
  const pp::Size& size() const { return size_; }

  // Sets the layout's total size.
  void set_size(const pp::Size& size) { size_ = size; }

  // Increases the layout's total height by |height|.
  void EnlargeHeight(int height);

  // Appends a rectangle of size |page_rect| to the layout. This will increase
  // the layout's height by the page's height, and increase the layout's width
  // to at least the page's width.
  void AppendPageRect(const pp::Size& page_rect);

 private:
  // Orientations are non-negative integers modulo 4.
  //
  // TODO(kmoon): Doesn't match return type of default_page_orientation().
  // Callers expect int, but internally, we want unsigned semantics. This will
  // be cleaned up when we switch to an enum.
  unsigned int default_page_orientation_ = 0;

  // Layout's total size.
  pp::Size size_;
};

}  // namespace chrome_pdf

#endif  // PDF_DOCUMENT_LAYOUT_H_
