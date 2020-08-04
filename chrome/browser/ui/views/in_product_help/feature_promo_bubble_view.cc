// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_view.h"

#include <memory>
#include <utility>

#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_params.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_timeout.h"
#include "components/variations/variations_associated_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace {

// The amount of time the promo should stay onscreen if the user
// never hovers over it.
constexpr base::TimeDelta kDelayDefault = base::TimeDelta::FromSeconds(10);

// The amount of time the promo should stay onscreen after the
// user stops hovering over it.
constexpr base::TimeDelta kDelayShort = base::TimeDelta::FromSeconds(3);

// The insets from the bubble border to the text inside.
constexpr gfx::Insets kBubbleContentsInsets(12, 16);

}  // namespace

FeaturePromoBubbleView::FeaturePromoBubbleView(FeaturePromoBubbleParams params)
    : BubbleDialogDelegateView(params.anchor_view, params.arrow),
      activation_action_(params.activation_action),
      feature_promo_bubble_timeout_(std::move(params.timeout)),
      preferred_width_(params.preferred_width) {
  DCHECK(params.anchor_view);
  UseCompactMargins();

  // If the timeout was not explicitly specified, use the default values.
  if (!feature_promo_bubble_timeout_) {
    feature_promo_bubble_timeout_ =
        std::make_unique<FeaturePromoBubbleTimeout>(kDelayDefault, kDelayShort);
  }

  const base::string16 body_text =
      l10n_util::GetStringUTF16(params.body_string_specifier);

  // Feature promos are purely informational. We can skip reading the UI
  // elements inside the bubble and just have the information announced when the
  // bubble shows. To do so, we change the a11y tree to make this a leaf node
  // and set the name to the message we want to announce.
  GetViewAccessibility().OverrideIsLeaf(true);
  if (!params.screenreader_string_specifier) {
    accessible_name_ = body_text;
  } else if (params.feature_accelerator) {
    accessible_name_ = l10n_util::GetStringFUTF16(
        *params.screenreader_string_specifier,
        params.feature_accelerator->GetShortcutText());
  } else {
    accessible_name_ =
        l10n_util::GetStringUTF16(*params.screenreader_string_specifier);
  }

  // We get the theme provider from the anchor view since our widget hasn't been
  // created yet.
  const ui::ThemeProvider* theme_provider =
      params.anchor_view->GetThemeProvider();
  DCHECK(theme_provider);

  const SkColor background_color = theme_provider->GetColor(
      ThemeProperties::COLOR_FEATURE_PROMO_BUBBLE_BACKGROUND);
  const SkColor text_color = theme_provider->GetColor(
      ThemeProperties::COLOR_FEATURE_PROMO_BUBBLE_TEXT);

  auto box_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kBubbleContentsInsets, 0);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  SetLayoutManager(std::move(box_layout));

  if (params.title_string_specifier.has_value()) {
    auto* title_label = AddChildView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(params.title_string_specifier.value())));
    title_label->SetBackgroundColor(background_color);
    title_label->SetEnabledColor(text_color);
    title_label->SetFontList(views::style::GetFont(
        views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY));

    if (params.preferred_width.has_value()) {
      title_label->SetMultiLine(true);
      title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    }
  }

  auto* body_label = AddChildView(std::make_unique<views::Label>(body_text));
  body_label->SetBackgroundColor(background_color);
  body_label->SetEnabledColor(text_color);

  if (params.preferred_width.has_value()) {
    body_label->SetMultiLine(true);
    body_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }

  if (params.activation_action ==
      FeaturePromoBubbleParams::ActivationAction::DO_NOT_ACTIVATE) {
    SetCanActivate(false);
    set_shadow(views::BubbleBorder::BIG_SHADOW);
  }

  set_margins(gfx::Insets());
  set_title_margins(gfx::Insets());
  SetButtons(ui::DIALOG_BUTTON_NONE);

  set_color(background_color);

  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(this);

  GetBubbleFrameView()->SetCornerRadius(
      ChromeLayoutProvider::Get()->GetCornerRadiusMetric(views::EMPHASIS_HIGH));

  widget->Show();
  feature_promo_bubble_timeout_->OnBubbleShown(this);
}

FeaturePromoBubbleView::~FeaturePromoBubbleView() = default;

// static
FeaturePromoBubbleView* FeaturePromoBubbleView::Create(
    FeaturePromoBubbleParams params) {
  return new FeaturePromoBubbleView(std::move(params));
}

void FeaturePromoBubbleView::CloseBubble() {
  GetWidget()->Close();
}

bool FeaturePromoBubbleView::OnMousePressed(const ui::MouseEvent& event) {
  base::RecordAction(
      base::UserMetricsAction("InProductHelp.Promos.BubbleClicked"));
  return false;
}

void FeaturePromoBubbleView::OnMouseEntered(const ui::MouseEvent& event) {
  feature_promo_bubble_timeout_->OnMouseEntered();
}

void FeaturePromoBubbleView::OnMouseExited(const ui::MouseEvent& event) {
  feature_promo_bubble_timeout_->OnMouseExited();
}

gfx::Rect FeaturePromoBubbleView::GetBubbleBounds() {
  gfx::Rect bounds = BubbleDialogDelegateView::GetBubbleBounds();
  if (activation_action_ ==
      FeaturePromoBubbleParams::ActivationAction::DO_NOT_ACTIVATE) {
    if (base::i18n::IsRTL())
      bounds.Offset(5, 0);
    else
      bounds.Offset(-5, 0);
  }
  return bounds;
}

ax::mojom::Role FeaturePromoBubbleView::GetAccessibleWindowRole() {
  // Since we don't have any controls for the user to interact with (we're just
  // an information bubble), override our role to kAlert.
  return ax::mojom::Role::kAlert;
}

base::string16 FeaturePromoBubbleView::GetAccessibleWindowTitle() const {
  return accessible_name_;
}

gfx::Size FeaturePromoBubbleView::CalculatePreferredSize() const {
  if (preferred_width_.has_value()) {
    return gfx::Size(preferred_width_.value(),
                     GetHeightForWidth(preferred_width_.value()));
  } else {
    return View::CalculatePreferredSize();
  }
}
