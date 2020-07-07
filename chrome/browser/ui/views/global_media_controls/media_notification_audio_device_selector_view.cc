// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_notification_audio_device_selector_view.h"

#include "base/strings/string16.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_container_impl_view.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/vector_icons.h"

namespace {

// Constants for this view
constexpr int kPaddingBetweenContainers = 10;

// Constants for the expand button and its container
// The container for the expand button will take up a fixed amount of
// space in this view. The leftover space will be given to the container
// for device selection buttons.
constexpr int kExpandButtonContainerWidth = 45;
constexpr int kExpandButtonSize = 20;
constexpr int kExpandButtonBorderThickness = 1;
constexpr int kExpandButtonBorderCornerRadius = 2;

// Constants for the device buttons and their container
constexpr int kPaddingBetweenDeviceButtons = 5;
constexpr int kDeviceButtonIconSize = 16;
constexpr gfx::Insets kDeviceButtonContainerInsets = gfx::Insets(0, 10, 0, 0);
constexpr gfx::Insets kDeviceButtonInsets = gfx::Insets(5);

}  // anonymous namespace

MediaNotificationAudioDeviceSelectorView::
    MediaNotificationAudioDeviceSelectorView(
        MediaNotificationContainerImplView* container,
        gfx::Size size)
    : container_(container) {
  DCHECK(container_);
  SetPreferredSize(size);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kPaddingBetweenContainers));

  auto device_button_container_width =
      size.width() - kExpandButtonContainerWidth;
  auto device_button_container = std::make_unique<views::View>();
  device_button_container->SetPreferredSize(
      gfx::Size(device_button_container_width, size.height()));
  auto* device_button_container_layout =
      device_button_container->SetLayoutManager(
          std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kHorizontal,
              kDeviceButtonContainerInsets, kPaddingBetweenDeviceButtons));
  device_button_container_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);
  device_button_container_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  device_button_container_ = AddChildView(std::move(device_button_container));

  auto expand_button_container = std::make_unique<views::View>();
  auto* expand_button_container_layout =
      expand_button_container->SetLayoutManager(
          std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kHorizontal));
  expand_button_container_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  expand_button_container_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  expand_button_container->SetPreferredSize(
      gfx::Size(kExpandButtonContainerWidth, size.height()));
  expand_button_container_ = AddChildView(std::move(expand_button_container));

  auto expand_button = views::CreateVectorToggleImageButton(this);
  expand_button->SetPreferredSize(
      gfx::Size(kExpandButtonSize, kExpandButtonSize));
  expand_button_ =
      expand_button_container_->AddChildView(std::move(expand_button));
  expand_button_->SetBorder(views::CreateRoundedRectBorder(
      kExpandButtonBorderThickness, kExpandButtonBorderCornerRadius,
      SK_ColorLTGRAY));
  views::SetImageFromVectorIcon(expand_button_, kKeyboardArrowDownIcon,
                                kExpandButtonSize, SK_ColorBLACK);
  views::SetToggledImageFromVectorIconWithColor(
      expand_button_, kKeyboardArrowUpIcon, kExpandButtonSize, SK_ColorBLACK,
      SK_ColorBLACK);
}

void MediaNotificationAudioDeviceSelectorView::UpdateAvailableAudioDevices(
    const media::AudioDeviceDescriptions& device_descriptions) {}

void MediaNotificationAudioDeviceSelectorView::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {}

void MediaNotificationAudioDeviceSelectorView::CreateDeviceButton(
    const media::AudioDeviceDescription& device_description) {
  auto button = views::MdTextButton::Create(
      this, base::UTF8ToUTF16(device_description.device_name.c_str()));
  button->SetImage(views::Button::ButtonState::STATE_NORMAL,
                   gfx::CreateVectorIcon(vector_icons::kHeadsetIcon,
                                         kDeviceButtonIconSize, SK_ColorBLACK));

  // I'm not sure if this border should be used with a MD button, but it
  // looks really nice.
  // TODO(noahrose): Investigate other border options.
  auto border = std::make_unique<views::LabelButtonBorder>();
  border->set_insets(kDeviceButtonInsets);
  border->set_color(SK_ColorLTGRAY);
  button->SetBorder(std::move(border));

  device_button_container_->AddChildView(std::move(button));
}
