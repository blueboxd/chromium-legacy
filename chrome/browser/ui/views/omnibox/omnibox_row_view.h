// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_ROW_VIEW_H_

#include "base/optional.h"
#include "base/strings/string16.h"
#include "ui/views/view.h"

class OmniboxResultView;

// The View that's a direct child of the OmniboxPopupContentsView, one per row.
// This, in turn, has a child OmniboxResultView and an optional header that is
// painted right above it. The header is not a child of OmniboxResultView
// because it's logically not part of the result view:
//  - Hovering the header doesn't highlight the result view.
//  - Clicking the header doesn't navigate to the match.
//  - It's the header for multiple matches, it's just painted above this row.
class OmniboxRowView : public views::View {
 public:
  explicit OmniboxRowView(std::unique_ptr<OmniboxResultView> result_view);

  // Sets the header that appears above this row. |suggestion_group_id| can be
  // base::nullopt, and that will hide the header.
  void SetHeader(base::Optional<int> suggestion_group_id,
                 const base::string16& header_text);

  // The result view associated with this row.
  OmniboxResultView* result_view() const { return result_view_; }

 private:
  // Non-owning pointer to the result view for this row. This is never nullptr.
  OmniboxResultView* result_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_ROW_VIEW_H_
