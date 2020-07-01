// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_service_delegate.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/ui/views/sharesheet_bubble_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/view.h"

namespace sharesheet {

SharesheetServiceDelegate::SharesheetServiceDelegate(
    content::WebContents* web_contents,
    views::View* bubble_anchor_view)
    : sharesheet_bubble_view_(
          std::make_unique<SharesheetBubbleView>(bubble_anchor_view)) {
  Profile* const profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  sharesheet_service_ = std::make_unique<SharesheetService>(profile);
}

SharesheetServiceDelegate::~SharesheetServiceDelegate() = default;

void SharesheetServiceDelegate::ShowBubble() {
  sharesheet_bubble_view_->ShowBubble();
}

}  // namespace sharesheet
