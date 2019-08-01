// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/accessibility.h"

#include "pdf/pdf_engine.h"
#include "ppapi/c/private/ppb_pdf.h"

namespace chrome_pdf {

bool GetAccessibilityInfo(
    PDFEngine* engine,
    int32_t page_index,
    PP_PrivateAccessibilityPageInfo* page_info,
    std::vector<PP_PrivateAccessibilityTextRunInfo>* text_runs,
    std::vector<PP_PrivateAccessibilityCharInfo>* chars,
    std::vector<PP_PrivateAccessibilityLinkInfo>* links,
    std::vector<PP_PrivateAccessibilityImageInfo>* images) {
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
    PP_PrivateAccessibilityTextRunInfo text_run_info;
    pp::FloatRect bounds;
    engine->GetTextRunInfo(page_index, char_index, &text_run_info.len,
                           &text_run_info.font_size, &bounds);
    DCHECK_LE(char_index + text_run_info.len,
              static_cast<uint32_t>(char_count));
    text_run_info.direction = PP_PRIVATEDIRECTION_LTR;
    text_run_info.bounds = bounds;
    text_runs->push_back(text_run_info);

    // We need to provide enough information to draw a bounding box
    // around any arbitrary text range, but the bounding boxes of characters
    // we get from PDFium don't necessarily "line up". Walk through the
    // characters in each text run and let the width of each character be
    // the difference between the x coordinate of one character and the
    // x coordinate of the next. The rest of the bounds of each character
    // can be computed from the bounds of the text run.
    pp::FloatRect char_bounds = engine->GetCharBounds(page_index, char_index);
    for (uint32_t i = 0; i < text_run_info.len - 1; i++) {
      DCHECK_LT(char_index + i + 1, static_cast<uint32_t>(char_count));
      pp::FloatRect next_char_bounds =
          engine->GetCharBounds(page_index, char_index + i + 1);
      (*chars)[char_index + i].char_width =
          next_char_bounds.x() - char_bounds.x();
      char_bounds = next_char_bounds;
    }
    (*chars)[char_index + text_run_info.len - 1].char_width =
        char_bounds.width();

    char_index += text_run_info.len;
  }

  page_info->text_run_count = text_runs->size();
  // TODO(crbug.com/981448): Populate |links| and |images|.
  page_info->link_count = links->size();
  page_info->image_count = images->size();
  return true;
}

}  // namespace chrome_pdf
