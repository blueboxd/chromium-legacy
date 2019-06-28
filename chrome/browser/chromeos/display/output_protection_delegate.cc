// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/output_protection_delegate.h"

#include "base/bind_helpers.h"
#include "chrome/browser/chromeos/display/output_protection_controller_ash.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"

namespace chromeos {

OutputProtectionDelegate::Controller::Controller() = default;
OutputProtectionDelegate::Controller::~Controller() = default;

OutputProtectionDelegate::OutputProtectionDelegate(aura::Window* window)
    : window_(window),
      display_id_(
          display::Screen::GetScreen()->GetDisplayNearestWindow(window).id()) {
  DCHECK(window_);

  window_->AddObserver(this);
  display::Screen::GetScreen()->AddObserver(this);
}

OutputProtectionDelegate::~OutputProtectionDelegate() {
  if (!window_)
    return;

  display::Screen::GetScreen()->RemoveObserver(this);
  window_->RemoveObserver(this);
}

void OutputProtectionDelegate::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  // Switching the primary display (either by user or by going into docked
  // mode), as well as changing mirror mode may change the display on which
  // the window resides without actually changing the window hierarchy (i.e.
  // the root window is still the same). Hence we need to watch out for these
  // situations and update |display_id_| if needed.
  if (!(changed_metrics &
        (display::DisplayObserver::DISPLAY_METRIC_PRIMARY |
         display::DisplayObserver::DISPLAY_METRIC_MIRROR_STATE))) {
    return;
  }

  OnWindowMayHaveMovedToAnotherDisplay();
}

void OutputProtectionDelegate::OnWindowHierarchyChanged(
    const aura::WindowObserver::HierarchyChangeParams& params) {
  OnWindowMayHaveMovedToAnotherDisplay();
}

void OutputProtectionDelegate::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window, window_);
  display::Screen::GetScreen()->RemoveObserver(this);
  window_->RemoveObserver(this);
  window_ = nullptr;
}

void OutputProtectionDelegate::QueryStatus(
    Controller::QueryStatusCallback callback) {
  if (!InitializeControllerIfNecessary()) {
    std::move(callback).Run(/*success=*/false,
                            display::DISPLAY_CONNECTION_TYPE_NONE,
                            display::CONTENT_PROTECTION_METHOD_NONE);
    return;
  }

  controller_->QueryStatus(display_id_, std::move(callback));
}

void OutputProtectionDelegate::SetProtection(
    uint32_t protection_mask,
    Controller::SetProtectionCallback callback) {
  if (!InitializeControllerIfNecessary()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  controller_->SetProtection(display_id_, protection_mask, std::move(callback));
  protection_mask_ = protection_mask;
}

void OutputProtectionDelegate::OnWindowMayHaveMovedToAnotherDisplay() {
  DCHECK(window_);
  int64_t new_display_id =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window_).id();

  if (display_id_ == new_display_id)
    return;

  if (protection_mask_ != display::CONTENT_PROTECTION_METHOD_NONE) {
    DCHECK(controller_);
    controller_->SetProtection(new_display_id, protection_mask_,
                               base::DoNothing());
    controller_->SetProtection(display_id_,
                               display::CONTENT_PROTECTION_METHOD_NONE,
                               base::DoNothing());
  }
  display_id_ = new_display_id;
}

bool OutputProtectionDelegate::InitializeControllerIfNecessary() {
  if (!window_)
    return false;

  if (!controller_)
    controller_ = std::make_unique<OutputProtectionControllerAsh>();

  return true;
}

}  // namespace chromeos
