// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_ICON_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/sharing/sharing_icon_view.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/vector_icon_types.h"

// The location bar icon to show the click to call bubble where the user can
// choose to send a phone number to a target device or use an OS handler app.
class ClickToCallIconView : public SharingIconView {
 public:
  explicit ClickToCallIconView(PageActionIconView::Delegate* delegate);
  ~ClickToCallIconView() override;

  views::BubbleDialogDelegateView* GetBubble() const override;
  base::string16 GetTextForTooltipAndAccessibleName() const override;

 protected:
  const gfx::VectorIcon& GetVectorIcon() const override;

  // SharingIconView
  SharingUiController* GetController() const override;
  base::Optional<int> GetSuccessMessageId() const override;

  DISALLOW_COPY_AND_ASSIGN(ClickToCallIconView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_ICON_VIEW_H_
