// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/pdf_accessibility_shared.h"

namespace ppapi {

PdfAccessibilityTextStyleInfo::PdfAccessibilityTextStyleInfo() = default;

PdfAccessibilityTextStyleInfo::PdfAccessibilityTextStyleInfo(
    const PP_PrivateAccessibilityTextStyleInfo& style)
    : font_name(std::string(style.font_name, style.font_name_length)),
      font_weight(style.font_weight),
      render_mode(style.render_mode),
      font_size(style.font_size),
      fill_color(style.fill_color),
      stroke_color(style.stroke_color),
      is_italic(style.is_italic),
      is_bold(style.is_bold) {}

PdfAccessibilityTextStyleInfo::PdfAccessibilityTextStyleInfo(
    PdfAccessibilityTextStyleInfo&& other) = default;

PdfAccessibilityTextStyleInfo::~PdfAccessibilityTextStyleInfo() = default;

PdfAccessibilityTextRunInfo::PdfAccessibilityTextRunInfo() = default;

PdfAccessibilityTextRunInfo::PdfAccessibilityTextRunInfo(
    const PP_PrivateAccessibilityTextRunInfo& text_run)
    : len(text_run.len),
      bounds(text_run.bounds),
      direction(text_run.direction),
      style(text_run.style) {}

PdfAccessibilityTextRunInfo::PdfAccessibilityTextRunInfo(
    PdfAccessibilityTextRunInfo&& other) = default;

PdfAccessibilityTextRunInfo::~PdfAccessibilityTextRunInfo() = default;

PdfAccessibilityLinkInfo::PdfAccessibilityLinkInfo() = default;

PdfAccessibilityLinkInfo::PdfAccessibilityLinkInfo(
    const PP_PrivateAccessibilityLinkInfo& link)
    : url(std::string(link.url, link.url_length)),
      index_in_page(link.index_in_page),
      text_run_index(link.text_run_index),
      text_run_count(link.text_run_count),
      bounds(link.bounds) {}

PdfAccessibilityLinkInfo::~PdfAccessibilityLinkInfo() = default;

PdfAccessibilityImageInfo::PdfAccessibilityImageInfo() = default;

PdfAccessibilityImageInfo::PdfAccessibilityImageInfo(
    const PP_PrivateAccessibilityImageInfo& image)
    : alt_text(std::string(image.alt_text, image.alt_text_length)),
      text_run_index(image.text_run_index),
      bounds(image.bounds) {}

PdfAccessibilityImageInfo::~PdfAccessibilityImageInfo() = default;

PdfAccessibilityHighlightInfo::PdfAccessibilityHighlightInfo() = default;

PdfAccessibilityHighlightInfo::~PdfAccessibilityHighlightInfo() = default;

PdfAccessibilityHighlightInfo::PdfAccessibilityHighlightInfo(
    const PP_PrivateAccessibilityHighlightInfo& highlight)
    : note_text(std::string(highlight.note_text, highlight.note_text_length)),
      index_in_page(highlight.index_in_page),
      text_run_index(highlight.text_run_index),
      text_run_count(highlight.text_run_count),
      bounds(highlight.bounds),
      color(highlight.color) {}

PdfAccessibilityTextFieldInfo::PdfAccessibilityTextFieldInfo() = default;

PdfAccessibilityTextFieldInfo::~PdfAccessibilityTextFieldInfo() = default;

PdfAccessibilityTextFieldInfo::PdfAccessibilityTextFieldInfo(
    const PP_PrivateAccessibilityTextFieldInfo& text_field)
    : name(std::string(text_field.name, text_field.name_length)),
      value(std::string(text_field.value, text_field.value_length)),
      is_read_only(text_field.is_read_only),
      is_required(text_field.is_required),
      is_password(text_field.is_password),
      index_in_page(text_field.index_in_page),
      text_run_index(text_field.text_run_index),
      bounds(text_field.bounds) {}

PdfAccessibilityPageObjects::PdfAccessibilityPageObjects() = default;

PdfAccessibilityPageObjects::PdfAccessibilityPageObjects(
    const PP_PrivateAccessibilityPageObjects& page_objects) {
  links.reserve(page_objects.link_count);
  for (size_t i = 0; i < page_objects.link_count; i++) {
    links.emplace_back(page_objects.links[i]);
  }

  images.reserve(page_objects.image_count);
  for (size_t i = 0; i < page_objects.image_count; i++) {
    images.emplace_back(page_objects.images[i]);
  }

  highlights.reserve(page_objects.highlight_count);
  for (size_t i = 0; i < page_objects.highlight_count; i++) {
    highlights.emplace_back(page_objects.highlights[i]);
  }

  text_fields.reserve(page_objects.text_field_count);
  for (size_t i = 0; i < page_objects.text_field_count; i++) {
    text_fields.emplace_back(page_objects.text_fields[i]);
  }
}

PdfAccessibilityPageObjects::~PdfAccessibilityPageObjects() = default;

}  // namespace ppapi
