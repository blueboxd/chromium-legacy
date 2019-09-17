// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/accessibility.h"

#include "pdf/pdf_engine.h"

namespace chrome_pdf {

bool GetAccessibilityInfo(
    PDFEngine* engine,
    int32_t page_index,
    PP_PrivateAccessibilityPageInfo* page_info,
    std::vector<PP_PrivateAccessibilityTextRunInfo>* text_runs,
    std::vector<PP_PrivateAccessibilityCharInfo>* chars,
    std::vector<pp::PDF::PrivateAccessibilityLinkInfo>* links,
    std::vector<pp::PDF::PrivateAccessibilityImageInfo>* images) {
  int page_count = engine->GetNumberOfPages();
  if (page_index < 0 || page_index >= page_count)
    return false;

  int char_count = engine->GetCharCount(page_index);

  // Treat a char count of -1 (error) as 0 (an empty page), since
  // other pages might have valid content.
  if (char_count < 0)
    char_count = 0;

  page_info->page_index = page_index;
  page_info->bounds = engine->GetPageBoundsRect(page_index);
  page_info->char_count = char_count;

  chars->resize(page_info->char_count);
  for (uint32_t i = 0; i < page_info->char_count; ++i) {
    (*chars)[i].unicode_character = engine->GetCharUnicode(page_index, i);
  }

  int char_index = 0;
  while (char_index < char_count) {
    base::Optional<PP_PrivateAccessibilityTextRunInfo> text_run_info_result =
        engine->GetTextRunInfo(page_index, char_index);
    DCHECK(text_run_info_result.has_value());
    const auto& text_run_info = text_run_info_result.value();
    uint32_t text_run_end = char_index + text_run_info.len;
    DCHECK_LE(text_run_end, static_cast<uint32_t>(char_count));
    text_runs->push_back(text_run_info);

    // We need to provide enough information to draw a bounding box
    // around any arbitrary text range, but the bounding boxes of characters
    // we get from PDFium don't necessarily "line up".
    // Example for LTR text direction: walk through the
    // characters in each text run and let the width of each character be
    // the difference between the x coordinate of one character and the
    // x coordinate of the next. The rest of the bounds of each character
    // can be computed from the bounds of the text run.
    // The same idea is used for RTL, TTB and BTT text direction.
    pp::FloatRect char_bounds = engine->GetCharBounds(page_index, char_index);
    for (uint32_t i = char_index; i < text_run_end - 1; i++) {
      DCHECK_LT(i + 1, static_cast<uint32_t>(char_count));
      pp::FloatRect next_char_bounds = engine->GetCharBounds(page_index, i + 1);
      double& char_width = (*chars)[i].char_width;
      switch (text_run_info.direction) {
        case PP_PRIVATEDIRECTION_NONE:
        case PP_PRIVATEDIRECTION_LTR:
          char_width = next_char_bounds.x() - char_bounds.x();
          break;
        case PP_PRIVATEDIRECTION_TTB:
          char_width = next_char_bounds.y() - char_bounds.y();
          break;
        case PP_PRIVATEDIRECTION_RTL:
          char_width = char_bounds.right() - next_char_bounds.right();
          break;
        case PP_PRIVATEDIRECTION_BTT:
          char_width = char_bounds.bottom() - next_char_bounds.bottom();
          break;
      }
      char_bounds = next_char_bounds;
    }
    double& char_width = (*chars)[text_run_end - 1].char_width;
    if (text_run_info.direction == PP_PRIVATEDIRECTION_BTT ||
        text_run_info.direction == PP_PRIVATEDIRECTION_TTB) {
      char_width = char_bounds.height();
    } else {
      char_width = char_bounds.width();
    }

    char_index += text_run_info.len;
  }

  page_info->text_run_count = text_runs->size();
  // TODO(crbug.com/981448): Populate |links| and |images|.
  page_info->link_count = links->size();
  page_info->image_count = images->size();
  return true;
}

}  // namespace chrome_pdf
