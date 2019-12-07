// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_tab_counter_button.h"

#include <memory>

#include "base/i18n/message_formatter.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_view_class_properties.h"
#include "chrome/browser/ui/views/feature_promos/feature_promo_colors.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"

namespace {

class TabCounterAnimator : public gfx::AnimationDelegate {
 public:
  explicit TabCounterAnimator(views::View* animated_view)
      : animated_view_(animated_view),
        starting_bounds_(animated_view->bounds()),
        target_bounds_(starting_bounds_.x(),
                       starting_bounds_.y() - 4,
                       starting_bounds_.width(),
                       starting_bounds_.height()) {
    animation_ = std::make_unique<gfx::ThrobAnimation>(this);
    animation_->SetTweenType(gfx::Tween::Type::FAST_OUT_SLOW_IN);
    animation_->SetThrobDuration(base::TimeDelta::FromMilliseconds(100));
  }
  ~TabCounterAnimator() override = default;

  void Animate() { animation_->StartThrobbing(1); }

  // AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override {
    gfx::Rect bounds_rect = gfx::Tween::RectValueBetween(
        animation->GetCurrentValue(), starting_bounds_, target_bounds_);
    animated_view_->SetBoundsRect(bounds_rect);
  }

 private:
  views::View* const animated_view_;
  const gfx::Rect starting_bounds_;
  const gfx::Rect target_bounds_;
  std::unique_ptr<gfx::ThrobAnimation> animation_;

  DISALLOW_COPY_AND_ASSIGN(TabCounterAnimator);
};

class TabCounterUpdater : public TabStripModelObserver {
 public:
  TabCounterUpdater(views::Button* button, views::Label* tab_counter)
      : button_(button), tab_counter_(tab_counter), animator_(tab_counter) {}
  ~TabCounterUpdater() override = default;

  void UpdateCounter(TabStripModel* model) {
    const int num_tabs = model->count();

    button_->SetTooltipText(
        base::i18n::MessageFormatter::FormatWithNumberedArgs(
            l10n_util::GetStringUTF16(IDS_TOOLTIP_WEBUI_TAB_STRIP_TAB_COUNTER),
            num_tabs));
    // TODO(999557): Have a 99+-style fallback to limit the max text width.
    tab_counter_->SetText(base::FormatNumber(num_tabs));
    animator_.Animate();
  }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    UpdateCounter(tab_strip_model);
  }

 private:
  views::Button* const button_;
  views::Label* const tab_counter_;
  TabCounterAnimator animator_;
};

class WebUITabCounterButton : public views::Button {
 public:
  explicit WebUITabCounterButton(views::ButtonListener* listener);
  ~WebUITabCounterButton() override;

  void UpdateColors();

  // views::Button:
  void AfterPropertyChange(const void* key, int64_t old_value) override;
  void AddLayerBeneathView(ui::Layer* new_layer) override;
  void RemoveLayerBeneathView(ui::Layer* old_layer) override;
  void OnThemeChanged() override;

  std::unique_ptr<TabCounterUpdater> counter_updater_;
  views::InkDropContainerView* ink_drop_container_;
  views::Label* label_;
};

WebUITabCounterButton::WebUITabCounterButton(views::ButtonListener* listener)
    : views::Button(listener) {}

WebUITabCounterButton::~WebUITabCounterButton() = default;

void WebUITabCounterButton::UpdateColors() {
  const ui::ThemeProvider* theme_provider = GetThemeProvider();
  const SkColor toolbar_color =
      theme_provider ? theme_provider->GetColor(ThemeProperties::COLOR_TOOLBAR)
                     : gfx::kPlaceholderColor;
  label_->SetBackgroundColor(toolbar_color);

  const SkColor normal_text_color =
      theme_provider
          ? theme_provider->GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON)
          : gfx::kPlaceholderColor;
  const SkColor current_text_color =
      GetProperty(kHasInProductHelpPromoKey)
          ? GetFeaturePromoHighlightColorForToolbar(theme_provider)
          : normal_text_color;

  label_->SetEnabledColor(current_text_color);
  label_->SetBorder(views::CreateRoundedRectBorder(
      2,
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::EMPHASIS_MEDIUM),
      current_text_color));
}

void WebUITabCounterButton::AfterPropertyChange(const void* key,
                                                int64_t old_value) {
  if (key != kHasInProductHelpPromoKey)
    return;
  UpdateColors();
}

void WebUITabCounterButton::AddLayerBeneathView(ui::Layer* new_layer) {
  ink_drop_container_->AddLayerBeneathView(new_layer);
}

void WebUITabCounterButton::RemoveLayerBeneathView(ui::Layer* old_layer) {
  ink_drop_container_->RemoveLayerBeneathView(old_layer);
}

void WebUITabCounterButton::OnThemeChanged() {
  UpdateColors();
  ConfigureInkDropForToolbar(this);
}

}  // namespace

std::unique_ptr<views::View> CreateWebUITabCounterButton(
    views::ButtonListener* listener,
    TabStripModel* tab_strip_model) {
  auto tab_counter = std::make_unique<WebUITabCounterButton>(listener);

  tab_counter->SetID(VIEW_ID_WEBUI_TAB_STRIP_TAB_COUNTER);

  // TODO(999557): Create a custom text style to get the correct size/weight.
  // TODO(999557): Figure out how to get the right font.
  tab_counter->SetProperty(views::kFlexBehaviorKey,
                           views::FlexSpecification::ForSizeRule(
                               views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred)
                               .WithOrder(1));

  const int button_height = GetLayoutConstant(TOOLBAR_BUTTON_HEIGHT);
  tab_counter->SetPreferredSize(gfx::Size(button_height, button_height));

  views::InkDropContainerView* ink_drop_container = tab_counter->AddChildView(
      std::make_unique<views::InkDropContainerView>());
  tab_counter->ink_drop_container_ = ink_drop_container;
  ink_drop_container->SetBoundsRect(tab_counter->GetLocalBounds());

  views::Label* label =
      tab_counter->AddChildView(std::make_unique<views::Label>());
  tab_counter->label_ = label;

  constexpr int kDesiredBorderHeight = 18;
  const int inset_height = (button_height - kDesiredBorderHeight) / 2;
  label->SetBounds(inset_height, inset_height, kDesiredBorderHeight,
                   kDesiredBorderHeight);

  tab_counter->counter_updater_ =
      std::make_unique<TabCounterUpdater>(tab_counter.get(), label);
  tab_strip_model->AddObserver(tab_counter->counter_updater_.get());
  tab_counter->counter_updater_->UpdateCounter(tab_strip_model);

  return tab_counter;
}
