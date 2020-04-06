// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_row_view.h"

#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "ui/views/layout/box_layout.h"

OmniboxRowView::OmniboxRowView(std::unique_ptr<OmniboxResultView> result_view) {
  DCHECK(result_view);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  result_view_ = AddChildView(std::move(result_view));
}

void OmniboxRowView::SetHeader(base::Optional<int> suggestion_group_id,
                               const base::string16& header_text) {
  // TODO(tommycli): Create a child header view here. Not implemented yet.
}
