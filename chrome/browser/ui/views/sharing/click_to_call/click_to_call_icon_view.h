// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_ICON_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/vector_icon_types.h"

namespace gfx {
class Canvas;
class ThrobAnimation;
}  // namespace gfx

namespace ui {
class Event;
}  // namespace ui

class ClickToCallSharingDialogController;

// The location bar icon to show the click to call bubble where the user can
// choose to send a phone number to a target device or use an OS handler app.
class ClickToCallIconView : public PageActionIconView {
 public:
  explicit ClickToCallIconView(PageActionIconView::Delegate* delegate);
  ~ClickToCallIconView() override;

  // PageActionIconView:
  views::BubbleDialogDelegateView* GetBubble() const override;
  bool Update() override;
  base::string16 GetTextForTooltipAndAccessibleName() const override;

  // views::Button:
  void PaintButtonContents(gfx::Canvas* canvas) override;

  // views::View:
  void OnThemeChanged() override;

  void StartLoadingAnimation();
  void StopLoadingAnimation();

 protected:
  // PageActionIconView:
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  const gfx::VectorIcon& GetVectorIconBadge() const override;
  bool IsTriggerableEvent(const ui::Event& event) override;
  double WidthMultiplier() const override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

 private:
  void UpdateInkDrop(bool activate);
  void UpdateLoaderColor();
  void UpdateOpacity();

  SkColor loader_color_;
  std::unique_ptr<gfx::ThrobAnimation> loading_animation_;
  bool show_error_ = false;
  ClickToCallSharingDialogController* last_controller_;

  DISALLOW_COPY_AND_ASSIGN(ClickToCallIconView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_ICON_VIEW_H_
