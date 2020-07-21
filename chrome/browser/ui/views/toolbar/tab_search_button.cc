// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/tab_search_button.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/tab_search/tab_search_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"

// TODO(crbug.com/1099917): This is a WIP. Implement more detail when design is
// final
TabSearchButton::TabSearchButton(Browser* browser)
    : ToolbarButton(this), browser_(browser) {
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_FIND));
}

void TabSearchButton::UpdateIcon() {
  UpdateIconsWithStandardColors(vector_icons::kFolderIcon);
}

void TabSearchButton::ButtonPressed(views::Button* sender,
                                    const ui::Event& event) {
  TabSearchBubbleView::CreateTabSearchBubble(browser_->profile(), this);
}
