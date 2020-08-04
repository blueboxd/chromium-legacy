// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_IN_PRODUCT_HELP_FEATURE_PROMO_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_IN_PRODUCT_HELP_FEATURE_PROMO_BUBBLE_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_params.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_timeout.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace gfx {
class Rect;
}

namespace ui {
class MouseEvent;
}  // namespace ui

// The FeaturePromoBubbleView is a special BubbleDialogDelegateView for
// in-product help which educates users about certain Chrome features in a
// deferred context.
class FeaturePromoBubbleView : public views::BubbleDialogDelegateView {
 public:
  ~FeaturePromoBubbleView() override;

  // Creates the promo. The returned pointer is only valid until the
  // widget is destroyed. It must not be manually deleted by the caller.
  static FeaturePromoBubbleView* Create(FeaturePromoBubbleParams params);

  // Closes the promo bubble.
  void CloseBubble();

 private:
  explicit FeaturePromoBubbleView(FeaturePromoBubbleParams params);

  // BubbleDialogDelegateView:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  gfx::Rect GetBubbleBounds() override;
  ax::mojom::Role GetAccessibleWindowRole() override;
  base::string16 GetAccessibleWindowTitle() const override;
  void UpdateHighlightedButton(bool highlighted) override {
    // Do nothing: the anchor for promo bubbles should not highlight.
  }
  gfx::Size CalculatePreferredSize() const override;

  const FeaturePromoBubbleParams::ActivationAction activation_action_;

  base::string16 accessible_name_;

  std::unique_ptr<FeaturePromoBubbleTimeout> feature_promo_bubble_timeout_;

  base::Optional<int> preferred_width_;

  DISALLOW_COPY_AND_ASSIGN(FeaturePromoBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_IN_PRODUCT_HELP_FEATURE_PROMO_BUBBLE_VIEW_H_
