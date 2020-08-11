// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_IN_PRODUCT_HELP_FEATURE_PROMO_CONTROLLER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_IN_PRODUCT_HELP_FEATURE_PROMO_CONTROLLER_VIEWS_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_controller.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class FeaturePromoBubbleView;
struct FeaturePromoBubbleParams;
class Profile;

namespace base {
struct Feature;
}

namespace feature_engagement {
class Tracker;
}

// Views implementation of FeaturePromoController.
class FeaturePromoControllerViews : public FeaturePromoController,
                                    public views::WidgetObserver {
 public:
  explicit FeaturePromoControllerViews(Profile* profile);
  ~FeaturePromoControllerViews() override;

  // Repositions the bubble (if showing) relative to the anchor view.
  // This should be called whenever the anchor view is potentially
  // moved. It is safe to call this if a bubble is not showing.
  void UpdateBubbleForAnchorBoundsChange();

  // FeaturePromoController:
  bool MaybeShowPromo(const base::Feature& iph_feature,
                      FeaturePromoBubbleParams params) override;
  bool BubbleIsShowing(const base::Feature& iph_feature) const override;
  void CloseBubble(const base::Feature& iph_feature) override;
  PromoHandle CloseBubbleAndContinuePromo(
      const base::Feature& iph_feature) override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  FeaturePromoBubbleView* promo_bubble_for_testing() { return promo_bubble_; }
  const FeaturePromoBubbleView* promo_bubble_for_testing() const {
    return promo_bubble_;
  }

 private:
  // Called when PromoHandle is destroyed to finish the promo.
  void FinishContinuedPromo() override;

  void HandleBubbleClosed();

  // IPH backend that is notified of user events and decides whether to
  // trigger IPH.
  feature_engagement::Tracker* const tracker_;

  // Non-null as long as a promo is showing. Corresponds to an IPH
  // feature registered with |tracker_|.
  const base::Feature* current_iph_feature_ = nullptr;

  // The bubble currently showing, if any.
  FeaturePromoBubbleView* promo_bubble_ = nullptr;

  // Stores the bubble anchor view so we can set/unset a highlight on
  // it.
  views::ViewTracker anchor_view_tracker_;

  ScopedObserver<views::Widget, views::WidgetObserver> widget_observer_{this};

  base::WeakPtrFactory<FeaturePromoController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_IN_PRODUCT_HELP_FEATURE_PROMO_CONTROLLER_VIEWS_H_
