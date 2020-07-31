// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FEATURE_PROMOS_FEATURE_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_FEATURE_PROMOS_FEATURE_PROMO_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
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

// Manages display of in-product help promos. All IPH displays in Top
// Chrome should go through here.
class FeaturePromoController : public views::WidgetObserver {
 public:
  explicit FeaturePromoController(Profile* profile);
  ~FeaturePromoController() override;

  // Starts the promo if possible. Returns whether it started.
  // |iph_feature| must be an IPH feature defined in
  // components/feature_engagement/public/feature_list.cc. Note that
  // this is different than the feature that the IPH is for.
  bool MaybeShowPromo(const base::Feature& iph_feature,
                      FeaturePromoBubbleParams params);

  // Returns whether a bubble is showing for the given IPH. Note that if
  // this is false, a promo might still be in progress; for example, a
  // promo may have continued into a menu in which case the bubble is no
  // longer showing.
  bool BubbleIsShowing(const base::Feature& iph_feature) const;

  // Close the bubble for |iph_feature| and end the promo. If no promo
  // is showing for |iph_feature|, or the promo has continued past the
  // bubble, calling this is an error.
  void CloseBubble(const base::Feature& iph_feature);

  class PromoHandle;

  // Like CloseBubble() but does not end the promo yet. The caller takes
  // ownership of the promo (e.g. to show a highlight in a menu or on a
  // button). The returned PromoHandle represents this ownership.
  PromoHandle CloseBubbleAndContinuePromo(const base::Feature& iph_feature);

  // Repositions the bubble (if showing) relative to the anchor view.
  // This should be called whenever the anchor view is potentially
  // moved. It is safe to call this if a bubble is not showing.
  void UpdateBubbleForAnchorBoundsChange();

  // When a caller wants to take ownership of the promo after a bubble
  // is closed, this handle is given. It must be dropped in a timely
  // fashion to ensure everything is cleaned up. If it isn't, it will
  // make the IPH backend think it's still shwoing and block all other
  // IPH indefinitely.
  class PromoHandle {
   public:
    explicit PromoHandle(base::WeakPtr<FeaturePromoController> controller);
    PromoHandle(PromoHandle&&);
    ~PromoHandle();

    PromoHandle& operator=(PromoHandle&&);

   private:
    base::WeakPtr<FeaturePromoController> controller_;
  };

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  FeaturePromoBubbleView* promo_bubble_for_testing() { return promo_bubble_; }
  const FeaturePromoBubbleView* promo_bubble_for_testing() const {
    return promo_bubble_;
  }

 private:
  // Called when PromoHandle is destroyed to finish the promo.
  void FinishContinuedPromo();

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

#endif  // CHROME_BROWSER_UI_VIEWS_FEATURE_PROMOS_FEATURE_PROMO_CONTROLLER_H_
