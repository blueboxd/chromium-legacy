// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_icon_linux_wrapper.h"

#include <memory>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/views/linux_ui/linux_ui.h"

#if defined(USE_DBUS)
#include "chrome/browser/ui/views/status_icons/status_icon_linux_dbus.h"
#endif

#if defined(USE_X11)
#include "chrome/browser/ui/views/status_icons/status_icon_linux_x11.h"
#endif

namespace {

// Prefix for app indicator ids
const char kAppIndicatorIdPrefix[] = "chrome_app_indicator_";

gfx::ImageSkia GetBestImageRep(const gfx::ImageSkia& image) {
  float best_scale = 0.0f;
  SkBitmap best_rep;
  for (const auto& rep : image.image_reps()) {
    if (rep.scale() > best_scale) {
      best_scale = rep.scale();
      best_rep = rep.GetBitmap();
    }
  }
  // All status icon implementations want the image in pixel coordinates, so use
  // a scale factor of 1.
  return gfx::ImageSkia(gfx::ImageSkiaRep(best_rep, 1.0f));
}

}  // namespace

StatusIconLinuxWrapper::StatusIconLinuxWrapper(
    std::unique_ptr<views::StatusIconLinux> status_icon,
    StatusIconType status_icon_type,
    const gfx::ImageSkia& image,
    const base::string16& tool_tip)
    : status_icon_(std::move(status_icon)),
      status_icon_type_(status_icon_type),
      image_(GetBestImageRep(image)),
      tool_tip_(tool_tip),
      menu_model_(nullptr) {
  status_icon_->SetDelegate(this);
}

StatusIconLinuxWrapper::~StatusIconLinuxWrapper() {
  if (menu_model_)
    menu_model_->RemoveObserver(this);
}

void StatusIconLinuxWrapper::SetImage(const gfx::ImageSkia& image) {
  image_ = GetBestImageRep(image);
  if (status_icon_)
    status_icon_->SetIcon(image_);
}

void StatusIconLinuxWrapper::SetToolTip(const base::string16& tool_tip) {
  tool_tip_ = tool_tip;
  if (status_icon_)
    status_icon_->SetToolTip(tool_tip);
}

void StatusIconLinuxWrapper::DisplayBalloon(
    const gfx::ImageSkia& icon,
    const base::string16& title,
    const base::string16& contents,
    const message_center::NotifierId& notifier_id) {
  notification_.DisplayBalloon(icon, title, contents, notifier_id);
}

void StatusIconLinuxWrapper::OnClick() {
  DispatchClickEvent();
}

bool StatusIconLinuxWrapper::HasClickAction() {
  return HasObservers();
}

const gfx::ImageSkia& StatusIconLinuxWrapper::GetImage() const {
  return image_;
}

const base::string16& StatusIconLinuxWrapper::GetToolTip() const {
  return tool_tip_;
}

ui::MenuModel* StatusIconLinuxWrapper::GetMenuModel() const {
  return menu_model_;
}

void StatusIconLinuxWrapper::OnImplInitializationFailed() {
  switch (status_icon_type_) {
    case kTypeDbus:
#if defined(USE_X11)
      status_icon_ = std::make_unique<StatusIconLinuxX11>();
      status_icon_type_ = kTypeX11;
      status_icon_->SetDelegate(this);
      return;
#else
      // Fallthrough needs to be omitted if USE_X11, otherwise clang will
      // complain about an unreachable fallthrough.
      FALLTHROUGH;
#endif
    case kTypeX11:
      status_icon_.reset();
      status_icon_type_ = kTypeOther;
      if (menu_model_)
        menu_model_->RemoveObserver(this);
      menu_model_ = nullptr;
      return;
    case kTypeOther:
      NOTREACHED();
  }
}

void StatusIconLinuxWrapper::OnMenuStateChanged() {
  if (status_icon_)
    status_icon_->RefreshPlatformContextMenu();
}

std::unique_ptr<StatusIconLinuxWrapper>
StatusIconLinuxWrapper::CreateWrappedStatusIcon(
    const gfx::ImageSkia& image,
    const base::string16& tool_tip) {
  if (base::FeatureList::IsEnabled(features::kEnableDbusAndX11StatusIcons)) {
#if defined(USE_DBUS)
    return base::WrapUnique(new StatusIconLinuxWrapper(
        std::make_unique<StatusIconLinuxDbus>(), kTypeDbus, image, tool_tip));
#elif defined(USE_X11)
    return base::WrapUnique(new StatusIconLinuxWrapper(
        std::make_unique<StatusIconLinuxX11>(), kTypeX11, image, tool_tip));
#endif
    return nullptr;
  }
  const views::LinuxUI* linux_ui = views::LinuxUI::instance();
  if (linux_ui) {
    auto status_icon =
        linux_ui->CreateLinuxStatusIcon(image, tool_tip, kAppIndicatorIdPrefix);
    if (status_icon) {
      return base::WrapUnique(
          new StatusIconLinuxWrapper(std::move(status_icon), kTypeOther,
                                     gfx::ImageSkia(), base::string16()));
    }
  }
  return nullptr;
}

void StatusIconLinuxWrapper::UpdatePlatformContextMenu(
    StatusIconMenuModel* model) {
  if (!status_icon_)
    return;

  // If a menu already exists, remove ourself from its observer list.
  if (menu_model_)
    menu_model_->RemoveObserver(this);

  status_icon_->UpdatePlatformContextMenu(model);
  menu_model_ = model;

  if (model)
    model->AddObserver(this);
}
