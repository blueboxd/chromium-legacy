// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/output_protection_delegate.h"

#include "base/bind_helpers.h"
#include "chrome/browser/chromeos/display/output_protection_controller_ash.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"

namespace chromeos {

namespace {

bool GetCurrentDisplayId(content::RenderFrameHost* rfh, int64_t* display_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(rfh);
  DCHECK(display_id);

  display::Screen* screen = display::Screen::GetScreen();
  if (!screen)
    return false;
  display::Display display =
      screen->GetDisplayNearestView(rfh->GetNativeView());
  *display_id = display.id();
  DCHECK_NE(*display_id, display::kInvalidDisplayId);

  return true;
}

}  // namespace

OutputProtectionDelegate::Controller::Controller() = default;
OutputProtectionDelegate::Controller::~Controller() = default;

OutputProtectionDelegate::OutputProtectionDelegate(int render_process_id,
                                                   int render_frame_id)
    : render_process_id_(render_process_id), render_frame_id_(render_frame_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  display::Screen::GetScreen()->AddObserver(this);
}

OutputProtectionDelegate::~OutputProtectionDelegate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  display::Screen::GetScreen()->RemoveObserver(this);
  if (window_)
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
  window_->RemoveObserver(this);
  window_ = nullptr;
}

void OutputProtectionDelegate::QueryStatus(
    Controller::QueryStatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

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
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!InitializeControllerIfNecessary()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  controller_->SetProtection(display_id_, protection_mask, std::move(callback));
  protection_mask_ = protection_mask;
}

void OutputProtectionDelegate::OnWindowMayHaveMovedToAnotherDisplay() {
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  if (!rfh) {
    DLOG(WARNING) << "RenderFrameHost is not alive.";
    return;
  }

  int64_t new_display_id = display::kInvalidDisplayId;
  if (!GetCurrentDisplayId(rfh, &new_display_id))
    return;

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
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (controller_)
    return true;

  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  if (!rfh) {
    DLOG(WARNING) << "RenderFrameHost is not alive.";
    return false;
  }

  int64_t display_id = display::kInvalidDisplayId;
  if (!GetCurrentDisplayId(rfh, &display_id))
    return false;

  aura::Window* window = rfh->GetNativeView();
  if (!window)
    return false;

  controller_ = std::make_unique<OutputProtectionControllerAsh>();

  display_id_ = display_id;
  window_ = window;
  window_->AddObserver(this);
  return true;
}

}  // namespace chromeos
