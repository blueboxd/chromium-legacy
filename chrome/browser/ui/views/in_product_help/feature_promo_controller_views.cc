// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/in_product_help/feature_promo_controller_views.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/chrome_view_class_properties.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_params.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_view.h"
#include "components/feature_engagement/public/tracker.h"

FeaturePromoControllerViews::FeaturePromoControllerViews(Profile* profile)
    : tracker_(
          feature_engagement::TrackerFactory::GetForBrowserContext(profile)) {
  DCHECK(tracker_);
}

FeaturePromoControllerViews::~FeaturePromoControllerViews() {
  if (!promo_bubble_) {
    DCHECK_EQ(current_iph_feature_, nullptr);
    return;
  }

  DCHECK(current_iph_feature_);

  promo_bubble_->GetWidget()->Close();
}

bool FeaturePromoControllerViews::MaybeShowPromo(
    const base::Feature& iph_feature,
    FeaturePromoBubbleParams params) {
  if (!tracker_->ShouldTriggerHelpUI(iph_feature))
    return false;

  // If the tracker says we should trigger, but we have a promo
  // currently showing, there is a bug somewhere in here.
  DCHECK(!current_iph_feature_);

  params.anchor_view->SetProperty(kHasInProductHelpPromoKey, true);
  anchor_view_tracker_.SetView(params.anchor_view);

  current_iph_feature_ = &iph_feature;
  promo_bubble_ = FeaturePromoBubbleView::Create(std::move(params));
  widget_observer_.Add(promo_bubble_->GetWidget());

  return true;
}

bool FeaturePromoControllerViews::BubbleIsShowing(
    const base::Feature& iph_feature) const {
  return promo_bubble_ && current_iph_feature_ == &iph_feature;
}

void FeaturePromoControllerViews::CloseBubble(
    const base::Feature& iph_feature) {
  DCHECK_EQ(&iph_feature, current_iph_feature_);
  DCHECK(promo_bubble_);
  promo_bubble_->GetWidget()->Close();
}

void FeaturePromoControllerViews::UpdateBubbleForAnchorBoundsChange() {
  if (!promo_bubble_)
    return;
  promo_bubble_->OnAnchorBoundsChanged();
}

FeaturePromoController::PromoHandle
FeaturePromoControllerViews::CloseBubbleAndContinuePromo(
    const base::Feature& iph_feature) {
  DCHECK_EQ(&iph_feature, current_iph_feature_);
  DCHECK(promo_bubble_);

  widget_observer_.Remove(promo_bubble_->GetWidget());
  promo_bubble_->GetWidget()->Close();
  promo_bubble_ = nullptr;

  if (anchor_view_tracker_.view())
    anchor_view_tracker_.view()->SetProperty(kHasInProductHelpPromoKey, false);

  return PromoHandle(weak_ptr_factory_.GetWeakPtr());
}

void FeaturePromoControllerViews::OnWidgetClosing(views::Widget* widget) {
  DCHECK(promo_bubble_);
  DCHECK_EQ(widget, promo_bubble_->GetWidget());
  HandleBubbleClosed();
}

void FeaturePromoControllerViews::OnWidgetDestroying(views::Widget* widget) {
  DCHECK(promo_bubble_);
  DCHECK_EQ(widget, promo_bubble_->GetWidget());
  HandleBubbleClosed();
}

void FeaturePromoControllerViews::FinishContinuedPromo() {
  DCHECK(current_iph_feature_);
  DCHECK(!promo_bubble_);
  tracker_->Dismissed(*current_iph_feature_);
  current_iph_feature_ = nullptr;
}

void FeaturePromoControllerViews::HandleBubbleClosed() {
  DCHECK(current_iph_feature_);

  tracker_->Dismissed(*current_iph_feature_);
  widget_observer_.Remove(promo_bubble_->GetWidget());

  current_iph_feature_ = nullptr;
  promo_bubble_ = nullptr;

  if (anchor_view_tracker_.view())
    anchor_view_tracker_.view()->SetProperty(kHasInProductHelpPromoKey, false);
}
