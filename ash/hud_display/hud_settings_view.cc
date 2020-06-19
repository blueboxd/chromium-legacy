// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/hud_settings_view.h"

#include "ash/hud_display/hud_properties.h"
#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace hud_display {

BEGIN_METADATA(HUDSettingsView)
METADATA_PARENT_CLASS(View)
END_METADATA()

constexpr SkColor HUDSettingsView::kDefaultColor;

HUDSettingsView::HUDSettingsView() {
  SetVisible(false);

  auto layout_manager = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  SetLayoutManager(std::move(layout_manager));
  SetBorder(views::CreateSolidBorder(1, kDefaultColor));

  // Use this to add checkboxes like:
  // add_checkbox(this, base::ASCIIToUTF16("test1"));
  auto add_checkbox = [](HUDSettingsView* self,
                         const base::string16& text) -> const views::Checkbox* {
    auto checkbox = std::make_unique<views::Checkbox>(text, self);
    const views::Checkbox* result = checkbox.get();
    checkbox->SetEnabledTextColors(kDefaultColor);
    checkbox->SetProperty(kHUDClickHandler, HTCLIENT);
    self->AddChildView(std::move(checkbox));
    return result;
  };

  // No checkboxes for now.
  ALLOW_UNUSED_LOCAL(add_checkbox);
}

HUDSettingsView::~HUDSettingsView() = default;

void HUDSettingsView::ButtonPressed(views::Button* sender,
                                    const ui::Event& /*event*/) {
  const views::Checkbox* checkbox = static_cast<views::Checkbox*>(sender);
  DVLOG(1) << "HUDSettingsView::ButtonPressed(): "
           << (checkbox->GetChecked() ? "Checked" : "Uncheked");
}

void HUDSettingsView::ToggleVisibility() {
  SetVisible(!GetVisible());
}

}  // namespace hud_display

}  // namespace ash
