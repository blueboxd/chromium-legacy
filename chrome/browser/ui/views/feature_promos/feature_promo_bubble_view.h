// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FEATURE_PROMOS_FEATURE_PROMO_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FEATURE_PROMOS_FEATURE_PROMO_BUBBLE_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/feature_promos/feature_promo_bubble_timeout.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace gfx {
class Rect;
}

namespace ui {
class Accelerator;
class MouseEvent;
}  // namespace ui

// The FeaturePromoBubbleView is a special BubbleDialogDelegateView for
// in-product help which educates users about certain Chrome features in a
// deferred context.
class FeaturePromoBubbleView : public views::BubbleDialogDelegateView {
 public:
  enum class ActivationAction {
    DO_NOT_ACTIVATE,
    ACTIVATE,
  };

  // Parameters to determine the promo's contents and appearance. Only
  // |body_string_specifier|, |anchor_view|, and |arrow| are required.
  struct CreateParams {
    CreateParams();
    ~CreateParams();

    CreateParams(CreateParams&&);

    // Promo contents:

    // The main promo text. Must be set to a valid string specifier.
    int body_string_specifier = -1;

    // Title shown larger at top of bubble. Optional.
    base::Optional<int> title_string_specifier;

    // String to be announced when bubble is shown. Optional.
    base::Optional<int> screenreader_string_specifier;

    // A keyboard accelerator to access the feature. If
    // |screenreader_string_specifier| is set and contains a
    // placeholder, this is filled in.
    base::Optional<ui::Accelerator> feature_accelerator;

    // Positioning and sizing:

    // View bubble is positioned relative to. Required.
    views::View* anchor_view = nullptr;

    // Determines position relative to |anchor_view|. Required. Note
    // that contrary to the name, no visible arrow is shown.
    views::BubbleBorder::Arrow arrow;

    // If set, determines the width of the bubble. Prefer the default if
    // possible.
    base::Optional<int> preferred_width;

    // Determines whether the bubble's widget can be activated, and
    // activates it on creation if so.
    ActivationAction activation_action = ActivationAction::DO_NOT_ACTIVATE;

    // Changes the bubble timeout. Intended for tests, avoid use.
    std::unique_ptr<FeaturePromoBubbleTimeout> timeout;
  };

  ~FeaturePromoBubbleView() override;

  // Creates the promo. The returned pointer is only valid until the
  // widget is destroyed. It must not be manually deleted by the caller.
  static FeaturePromoBubbleView* Create(CreateParams params);

  // Closes the promo bubble.
  void CloseBubble();

 private:
  explicit FeaturePromoBubbleView(CreateParams params);

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

  const ActivationAction activation_action_;

  base::string16 accessible_name_;

  std::unique_ptr<FeaturePromoBubbleTimeout> feature_promo_bubble_timeout_;

  base::Optional<int> preferred_width_;

  DISALLOW_COPY_AND_ASSIGN(FeaturePromoBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FEATURE_PROMOS_FEATURE_PROMO_BUBBLE_VIEW_H_
