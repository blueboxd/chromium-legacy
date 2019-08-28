// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHARING_SHARING_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SHARING_SHARING_ICON_VIEW_H_

#include "chrome/browser/sharing/sharing_ui_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "third_party/skia/include/core/SkColor.h"


// The location bar icon to show the sharing features bubble.
class SharingIconView : public PageActionIconView {
 public:
  explicit SharingIconView(PageActionIconView::Delegate* delegate);
  ~SharingIconView() override;

  void StartLoadingAnimation();
  void StopLoadingAnimation();

 protected:
  // PageActionIconView:
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  bool IsTriggerableEvent(const ui::Event& event) override;
  double WidthMultiplier() const override;
  const gfx::VectorIcon& GetVectorIconBadge() const override;
  bool Update() override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  void UpdateInkDrop(bool activate);
  void UpdateOpacity();
  bool IsLoadingAnimationVisible();

  // Get the supporting controller for this icon view.
  virtual SharingUiController* GetController() const = 0;

 private:
  SharingUiController* last_controller_ = nullptr;
  bool loading_animation_ = false;
  bool should_show_error_ = false;

  DISALLOW_COPY_AND_ASSIGN(SharingIconView);
};
#endif  // CHROME_BROWSER_UI_VIEWS_SHARING_SHARING_ICON_VIEW_H_
