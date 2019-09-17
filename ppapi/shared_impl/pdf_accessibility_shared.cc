// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/pdf_accessibility_shared.h"

namespace ppapi {

PdfAccessibilityLinkInfo::PdfAccessibilityLinkInfo() = default;

PdfAccessibilityLinkInfo::PdfAccessibilityLinkInfo(
    const PP_PrivateAccessibilityLinkInfo& link)
    : url(std::string(link.url, link.url_length)),
      index_in_page(link.index_in_page),
      text_run_index(link.text_run_index),
      text_run_count(link.text_run_count),
      bounds(link.bounds) {}

PdfAccessibilityLinkInfo::PdfAccessibilityLinkInfo(
    PdfAccessibilityLinkInfo&& other) = default;

PdfAccessibilityLinkInfo::~PdfAccessibilityLinkInfo() = default;

PdfAccessibilityImageInfo::PdfAccessibilityImageInfo() = default;

PdfAccessibilityImageInfo::PdfAccessibilityImageInfo(
    const PP_PrivateAccessibilityImageInfo& image)
    : alt_text(std::string(image.alt_text, image.alt_text_length)),
      text_run_index(image.text_run_index),
      bounds(image.bounds) {}

PdfAccessibilityImageInfo::PdfAccessibilityImageInfo(
    PdfAccessibilityImageInfo&& other) = default;

PdfAccessibilityImageInfo::~PdfAccessibilityImageInfo() = default;

}  // namespace ppapi
