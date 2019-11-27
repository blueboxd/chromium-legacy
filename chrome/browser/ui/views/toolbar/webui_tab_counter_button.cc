// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_tab_counter_button.h"

#include "base/i18n/message_formatter.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"

namespace {

class TabCounterLabelUpdater : public TabStripModelObserver {
 public:
  explicit TabCounterLabelUpdater(views::Label* tab_counter)
      : tab_counter_(tab_counter) {}
  ~TabCounterLabelUpdater() override = default;

  void UpdateCounter(TabStripModel* model) {
    const int num_tabs = model->count();

    tab_counter_->SetTooltipText(
        base::i18n::MessageFormatter::FormatWithNumberedArgs(
            l10n_util::GetStringUTF16(IDS_TOOLTIP_WEBUI_TAB_STRIP_TAB_COUNTER),
            num_tabs));
    // TODO(999557): Have a 99+-style fallback to limit the max text width.
    tab_counter_->SetText(base::FormatNumber(num_tabs));
  }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    UpdateCounter(tab_strip_model);
  }

 private:
  views::Label* tab_counter_;
};

class WebUITabCounterButton : public views::Button {
 public:
  explicit WebUITabCounterButton(views::ButtonListener* listener);
  ~WebUITabCounterButton() override;

  void AddLayerBeneathView(ui::Layer* new_layer) override;
  void RemoveLayerBeneathView(ui::Layer* old_layer) override;

  void OnThemeChanged() override;

  std::unique_ptr<TabCounterLabelUpdater> label_updater_;
  views::InkDropContainerView* ink_drop_container_;
  views::Label* label_;
};

}  // namespace

std::unique_ptr<views::View> CreateWebUITabCounterButton(
    views::ButtonListener* listener,
    TabStripModel* tab_strip_model) {
  auto tab_counter = std::make_unique<WebUITabCounterButton>(listener);

  tab_counter->SetID(VIEW_ID_WEBUI_TAB_STRIP_TAB_COUNTER);
  // TODO(crbug.com/1028827): replace this with production string and provide
  // tab count.
  tab_counter->SetAccessibleName(base::ASCIIToUTF16("Open tab strip"));

  // TODO(999557): Create a custom text style to get the correct size/weight.
  // TODO(999557): Figure out how to get the right font.
  tab_counter->SetProperty(views::kFlexBehaviorKey,
                           views::FlexSpecification::ForSizeRule(
                               views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred)
                               .WithOrder(1));

  // TODO(999557): also update this in response to touch mode changes.
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

  tab_counter->label_updater_ = std::make_unique<TabCounterLabelUpdater>(label);
  tab_strip_model->AddObserver(tab_counter->label_updater_.get());
  tab_counter->label_updater_->UpdateCounter(tab_strip_model);

  return tab_counter;
}

WebUITabCounterButton::WebUITabCounterButton(views::ButtonListener* listener)
    : views::Button(listener) {}

WebUITabCounterButton::~WebUITabCounterButton() = default;

void WebUITabCounterButton::AddLayerBeneathView(ui::Layer* new_layer) {
  ink_drop_container_->AddLayerBeneathView(new_layer);
}

void WebUITabCounterButton::RemoveLayerBeneathView(ui::Layer* old_layer) {
  ink_drop_container_->RemoveLayerBeneathView(old_layer);
}

void WebUITabCounterButton::OnThemeChanged() {
  const SkColor contents_color =
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON);
  label_->SetEnabledColor(contents_color);
  label_->SetBorder(views::CreateRoundedRectBorder(
      2,
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::EMPHASIS_MEDIUM),
      contents_color));

  ConfigureInkDropForToolbar(this);
}
