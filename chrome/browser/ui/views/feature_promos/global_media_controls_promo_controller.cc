// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/feature_promos/global_media_controls_promo_controller.h"

#include <memory>
#include <utility>

#include "base/time/time.h"
#include "chrome/browser/ui/views/feature_promos/feature_promo_bubble_timeout.h"
#include "chrome/browser/ui/views/feature_promos/feature_promo_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"

namespace {

constexpr base::TimeDelta kPromoHideDelay = base::TimeDelta::FromSeconds(5);

}  // anonymous namespace

GlobalMediaControlsPromoController::GlobalMediaControlsPromoController(
    BrowserView* browser_view)
    : browser_view_(browser_view) {}

void GlobalMediaControlsPromoController::ShowPromo() {
  // This shouldn't be called more than once. Check that state is fresh.
  DCHECK(!show_promo_called_);
  show_promo_called_ = true;

  DCHECK(!is_showing_);
  is_showing_ = true;

  auto* media_toolbar_button = browser_view_->toolbar()->media_button();

  // This should never be called when the toolbar button is not visible and
  // enabled.
  DCHECK(media_toolbar_button->GetVisible());
  DCHECK(media_toolbar_button->GetEnabled());

  // Here, we open the promo bubble.
  // TODO(https://crbug.com/991585): Also highlight the toolbar button.

  std::unique_ptr<FeaturePromoBubbleTimeout> feature_promo_bubble_timeout;
  if (!disable_bubble_timeout_for_test_) {
    feature_promo_bubble_timeout = std::make_unique<FeaturePromoBubbleTimeout>(
        kPromoHideDelay, base::TimeDelta());
  }

  // TODO(https://crbug.com/991585): Use IDS_GLOBAL_MEDIA_CONTROLS_PROMO here
  // instead. The reason we're going to use this placeholder here for now is
  // that the win10 builder fails the browser tests due to resource
  // whitelisting. Since the string isn't used in production code yet, it isn't
  // whitelisted and the browser test crashes.
  //
  // TODO(https://crbug.com/991585): Supply a screenreader string too.
  int string_specifier = IDS_GLOBAL_MEDIA_CONTROLS_ICON_TOOLTIP_TEXT;
  promo_bubble_ = FeaturePromoBubbleView::CreateOwned(
      media_toolbar_button, views::BubbleBorder::Arrow::TOP_RIGHT,
      FeaturePromoBubbleView::ActivationAction::DO_NOT_ACTIVATE,
      string_specifier, base::nullopt, base::nullopt,
      std::move(feature_promo_bubble_timeout));
  promo_bubble_->set_close_on_deactivate(false);
  promo_bubble_->GetWidget()->AddObserver(this);
}

void GlobalMediaControlsPromoController::OnMediaDialogOpened() {
  FinishPromo();
}

void GlobalMediaControlsPromoController::
    OnMediaToolbarButtonDisabledOrHidden() {
  FinishPromo();
}

void GlobalMediaControlsPromoController::OnWidgetDestroying(
    views::Widget* widget) {
  DCHECK(promo_bubble_);
  promo_bubble_ = nullptr;

  FinishPromo();
}

void GlobalMediaControlsPromoController::FinishPromo() {
  if (!is_showing_)
    return;

  if (promo_bubble_)
    promo_bubble_->GetWidget()->Close();

  is_showing_ = false;

  // TODO(https://crbug.com/991585): Inform the IPH service that the dialog was
  // closed.
}
