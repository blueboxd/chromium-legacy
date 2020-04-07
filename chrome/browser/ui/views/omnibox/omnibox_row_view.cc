// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_row_view.h"

#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

class OmniboxRowView::HeaderView : public views::Label {
 public:
  HeaderView() {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));

    header_text_ = AddChildView(std::make_unique<views::Label>());
  }

  void SetHeaderText(const base::string16& header_text) {
    header_text_->SetText(header_text);
  }

 private:
  // The Label containing the header text. This is never nullptr.
  views::Label* header_text_;
};

OmniboxRowView::OmniboxRowView(std::unique_ptr<OmniboxResultView> result_view) {
  DCHECK(result_view);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  result_view_ = AddChildView(std::move(result_view));
}

void OmniboxRowView::ShowHeader(base::Optional<int> suggestion_group_id,
                                const base::string16& header_text) {
  // Create the header (at index 0) if it doesn't exist.
  if (header_view_ == nullptr)
    header_view_ = AddChildViewAt(std::make_unique<HeaderView>(), 0);

  header_view_->SetHeaderText(header_text);
  header_view_->SetVisible(true);
}

void OmniboxRowView::HideHeader() {
  if (header_view_)
    header_view_->SetVisible(false);
}
