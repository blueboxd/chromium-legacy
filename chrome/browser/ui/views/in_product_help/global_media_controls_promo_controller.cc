// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/in_product_help/global_media_controls_promo_controller.h"

#include <memory>
#include <utility>

#include "base/time/time.h"
#include "chrome/browser/ui/in_product_help/global_media_controls_in_product_help.h"
#include "chrome/browser/ui/in_product_help/global_media_controls_in_product_help_factory.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_params.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_timeout.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"

namespace {

constexpr base::TimeDelta kPromoHideDelay = base::TimeDelta::FromSeconds(5);

}  // anonymous namespace

GlobalMediaControlsPromoController::GlobalMediaControlsPromoController(
    MediaToolbarButtonView* owner,
    Profile* profile)
    : owner_(owner), profile_(profile) {
  DCHECK(owner_);
  DCHECK(profile_);
}

GlobalMediaControlsPromoController::~GlobalMediaControlsPromoController() =
    default;

void GlobalMediaControlsPromoController::ShowPromo() {
  // This shouldn't be called more than once. Check that state is fresh.
  DCHECK(!show_promo_called_);
  show_promo_called_ = true;

  DCHECK(!is_showing_);
  is_showing_ = true;

  // This should never be called when the toolbar button is not visible and
  // enabled.
  DCHECK(owner_->GetVisible());
  DCHECK(owner_->GetEnabled());

  // Here, we open the promo bubble.
  // TODO(https://crbug.com/991585): Supply a screenreader string too.
  FeaturePromoBubbleParams bubble_params;
  bubble_params.body_string_specifier = IDS_GLOBAL_MEDIA_CONTROLS_PROMO;
  bubble_params.anchor_view = owner_;
  bubble_params.arrow = views::BubbleBorder::Arrow::TOP_RIGHT;
  if (!disable_bubble_timeout_for_test_) {
    bubble_params.timeout = std::make_unique<FeaturePromoBubbleTimeout>(
        kPromoHideDelay, base::TimeDelta());
  }

  promo_bubble_ = FeaturePromoBubbleView::Create(std::move(bubble_params));
  promo_bubble_->set_close_on_deactivate(false);
  observer_.Add(promo_bubble_->GetWidget());
}

void GlobalMediaControlsPromoController::OnMediaDialogOpened() {
  FinishPromo();
}

void GlobalMediaControlsPromoController::OnMediaButtonHidden() {
  FinishPromo();
}

void GlobalMediaControlsPromoController::OnMediaButtonDisabled() {
  FinishPromo();
}

void GlobalMediaControlsPromoController::OnWidgetDestroying(
    views::Widget* widget) {
  DCHECK(promo_bubble_);
  promo_bubble_ = nullptr;

  observer_.Remove(widget);

  FinishPromo();
}

void GlobalMediaControlsPromoController::FinishPromo() {
  if (!is_showing_)
    return;

  if (promo_bubble_)
    promo_bubble_->GetWidget()->Close();

  is_showing_ = false;

  owner_->OnPromoEnded();

  GlobalMediaControlsInProductHelpFactory::GetForProfile(profile_)
      ->HelpDismissed();
}
