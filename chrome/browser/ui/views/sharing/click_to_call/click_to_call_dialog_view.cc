// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing/click_to_call/click_to_call_dialog_view.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

#include "base/logging.h"

namespace {
// Icon sizes in DIP.
// TODO: Confirm the number with the team designer.
constexpr int kPrimaryIconSize = 20;
constexpr int kPrimaryIconBorderWidth = 6;

SkColor GetColorfromTheme() {
  const ui::NativeTheme* native_theme =
      ui::NativeTheme::GetInstanceForNativeUi();
  return native_theme->GetSystemColor(
      ui::NativeTheme::kColorId_DefaultIconColor);
}

std::unique_ptr<views::ImageView> CreateDeviceIcon(
    const sync_pb::SyncEnums::DeviceType device_type) {
  const gfx::VectorIcon* vector_icon;
  if (device_type == sync_pb::SyncEnums::TYPE_TABLET) {
    vector_icon = &kTabletIcon;
  } else {
    vector_icon = &kHardwareSmartphoneIcon;
  }
  gfx::ImageSkia image = gfx::CreateVectorIcon(*vector_icon, kPrimaryIconSize,
                                               GetColorfromTheme());
  auto icon_view = std::make_unique<views::ImageView>();
  icon_view->SetImage(image);
  icon_view->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(kPrimaryIconBorderWidth)));
  return icon_view;
}

}  // namespace

ClickToCallDialogView::ClickToCallDialogView(
    views::View* anchor_view,
    PageActionIconView* icon_view,
    content::WebContents* web_contents,
    std::unique_ptr<ClickToCallSharingDialogController> controller)
    : LocationBarBubbleDelegateView(anchor_view, gfx::Point(), web_contents),
      icon_view_(icon_view),
      controller_(std::move(controller)),
      devices_(controller_->GetSyncedDevices()),
      apps_(controller_->GetApps()) {}

ClickToCallDialogView::~ClickToCallDialogView() = default;

int ClickToCallDialogView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void ClickToCallDialogView::Init() {
  auto* provider = ChromeLayoutProvider::Get();
  set_margins(
      gfx::Insets(provider->GetDistanceMetric(
                      views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL),
                  0,
                  provider->GetDistanceMetric(
                      views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL),
                  0));
  SetLayoutManager(std::make_unique<views::FillLayout>());

  PopulateDialogView();
}

void ClickToCallDialogView::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  if (!sender || sender->tag() < 0)
    return;
  size_t index = static_cast<size_t>(sender->tag());
  DCHECK(index < devices_.size() + apps_.size());

  if (index < devices_.size()) {
    controller_->OnDeviceChosen(devices_[index], base::DoNothing());
    CloseBubble();
    return;
  }

  index -= devices_.size();

  if (index < apps_.size()) {
    controller_->OnAppChosen(apps_[index]);
    CloseBubble();
  }
}

void ClickToCallDialogView::PopulateDialogView() {
  dialog_buttons_.clear();
  int tag = 0;
  views::View* dialog_view = new views::View();
  dialog_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  AddChildView(dialog_view);
  // Devices:
  for (const auto& device : devices_) {
    auto dialog_button = std::make_unique<HoverButton>(
        this, CreateDeviceIcon(device.device_type()),
        base::UTF8ToUTF16(device.human_readable_name()),
        base::string16() /* No subtitle */);
    dialog_button->SetEnabled(true);
    dialog_button->set_tag(tag++);
    dialog_buttons_.push_back(
        dialog_view->AddChildView(std::move(dialog_button)));
  }

  // Apps:
  for (const auto& app : apps_) {
    auto dialog_button = std::make_unique<HoverButton>(this, app.name);
    // TODO(yasmo): Create Icon View.
    dialog_button->SetEnabled(true);
    dialog_button->set_tag(tag++);
    dialog_buttons_.push_back(
        dialog_view->AddChildView(std::move(dialog_button)));
  }

  // TODO: See if GetWidget can be not null:
  if (GetWidget())
    SizeToContents();
}

gfx::Size ClickToCallDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_BUBBLE_PREFERRED_WIDTH);
  return gfx::Size(width, GetHeightForWidth(width));
}

bool ClickToCallDialogView::ShouldShowCloseButton() const {
  return true;
}

base::string16 ClickToCallDialogView::GetWindowTitle() const {
  return base::UTF8ToUTF16(controller_->GetTitle());
}
