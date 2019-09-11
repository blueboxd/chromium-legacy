// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller_delegate.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace service_manager {
class Connector;
}  // namespace service_manager

class Browser;
class GlobalMediaControlsInProductHelp;
class GlobalMediaControlsPromoController;

// Media icon shown in the trusted area of toolbar. Its lifetime is tied to that
// of its parent ToolbarView. The icon is made visible when there is an active
// media session.
class MediaToolbarButtonView : public ToolbarButton,
                               public MediaToolbarButtonControllerDelegate,
                               public views::ButtonListener {
 public:
  MediaToolbarButtonView(const base::UnguessableToken& source_id,
                         service_manager::Connector* connector,
                         const Browser* browser);
  ~MediaToolbarButtonView() override;

  // MediaToolbarButtonControllerDelegate implementation.
  void Show() override;
  void Hide() override;
  void Enable() override;
  void Disable() override;

  // views::ButtonListener implementation.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::InkDropHostView implementation.
  SkColor GetInkDropBaseColor() const override;

  // Updates the icon image.
  void UpdateIcon();

  void ShowPromo();

  // Called when the in-product help bubble has gone away.
  void OnPromoEnded();

  GlobalMediaControlsPromoController* GetPromoControllerForTesting() {
    return &GetPromoController();
  }

 private:
  // Lazily constructs |promo_controller_| if necessary.
  GlobalMediaControlsPromoController& GetPromoController();

  // Informs the Global Media Controls in-product help that the GMC dialog was
  // opened.
  void InformIPHOfDialogShown();

  // Informs the Global Media Controls in-product help of the current button
  // state.
  void InformIPHOfButtonEnabled();
  void InformIPHOfButtonDisabledorHidden();

  // Shows the in-product help bubble.
  std::unique_ptr<GlobalMediaControlsPromoController> promo_controller_;

  // True if the in-product help bubble is currently showing.
  bool is_promo_showing_ = false;

  service_manager::Connector* const connector_;
  MediaToolbarButtonController controller_;
  const Browser* const browser_;
  GlobalMediaControlsInProductHelp* in_product_help_;

  DISALLOW_COPY_AND_ASSIGN(MediaToolbarButtonView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_VIEW_H_
