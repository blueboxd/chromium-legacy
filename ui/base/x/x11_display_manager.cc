// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_display_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/base/x/x11_display_util.h"
#include "ui/gfx/x/randr.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"

namespace ui {

namespace {

constexpr int kMinXrandrVersion = 103;  // Need at least xrandr version 1.3

}  // namespace

XDisplayManager::XDisplayManager(Delegate* delegate)
    : delegate_(delegate),
      connection_(x11::Connection::Get()),
      x_root_window_(connection_->default_screen().root),
      xrandr_version_(GetXrandrVersion()),
      workspace_handler_(this) {}

XDisplayManager::~XDisplayManager() = default;

void XDisplayManager::Init() {
  if (IsXrandrAvailable()) {
    auto& randr = connection_->randr();
    xrandr_event_base_ = randr.first_event();

    randr.SelectInput(
        {x_root_window_, x11::RandR::NotifyMask::ScreenChange |
                             x11::RandR::NotifyMask::OutputChange |
                             x11::RandR::NotifyMask::CrtcChange});
  }
  FetchDisplayList();
}

// Need at least xrandr version 1.3
bool XDisplayManager::IsXrandrAvailable() const {
  return xrandr_version_ >= kMinXrandrVersion;
}

display::Display XDisplayManager::GetPrimaryDisplay() const {
  DCHECK(!displays_.empty());
  return displays_[primary_display_index_];
}

void XDisplayManager::AddObserver(display::DisplayObserver* observer) {
  change_notifier_.AddObserver(observer);
}

void XDisplayManager::RemoveObserver(display::DisplayObserver* observer) {
  change_notifier_.RemoveObserver(observer);
}

bool XDisplayManager::CanProcessEvent(const x11::Event& x11_event) {
  const XEvent& xev = x11_event.xlib_event();
  return xev.type - xrandr_event_base_ ==
             x11::RandR::ScreenChangeNotifyEvent::opcode ||
         xev.type - xrandr_event_base_ == x11::RandR::NotifyEvent::opcode ||
         (xev.type == PropertyNotify &&
          static_cast<x11::Window>(xev.xproperty.window) == x_root_window_ &&
          xev.xproperty.atom ==
              static_cast<uint32_t>(gfx::GetAtom("_NET_WORKAREA")));
}

bool XDisplayManager::ProcessEvent(x11::Event* x11_event) {
  DCHECK(x11_event);
  XEvent* xev = &x11_event->xlib_event();
  int ev_type = xev->type - xrandr_event_base_;
  if (ev_type == x11::RandR::ScreenChangeNotifyEvent::opcode) {
    // Pass the event through to xlib.
    XRRUpdateConfiguration(xev);
    return true;
  }
  if (ev_type == x11::RandR::NotifyEvent::opcode ||
      (xev->type == PropertyNotify &&
       xev->xproperty.atom ==
           static_cast<uint32_t>(gfx::GetAtom("_NET_WORKAREA")))) {
    DispatchDelayedDisplayListUpdate();
    return true;
  }
  return false;
}

void XDisplayManager::SetDisplayList(std::vector<display::Display> displays) {
  displays_ = std::move(displays);
  delegate_->OnXDisplayListUpdated();
}

// Talks to xrandr to get the information of the outputs for a screen and
// updates display::Display list. The minimum required version of xrandr is
// 1.3.
void XDisplayManager::FetchDisplayList() {
  std::vector<display::Display> displays;
  float scale = delegate_->GetXDisplayScaleFactor();
  if (IsXrandrAvailable()) {
    displays = BuildDisplaysFromXRandRInfo(xrandr_version_, scale,
                                           &primary_display_index_);
  } else {
    displays = GetFallbackDisplayList(scale);
  }
  SetDisplayList(std::move(displays));
}

void XDisplayManager::OnCurrentWorkspaceChanged(
    const std::string& new_workspace) {
  change_notifier_.NotifyCurrentWorkspaceChanged(new_workspace);
}

void XDisplayManager::UpdateDisplayList() {
  std::vector<display::Display> old_displays = displays_;
  FetchDisplayList();
  change_notifier_.NotifyDisplaysChanged(old_displays, displays_);
}

void XDisplayManager::DispatchDelayedDisplayListUpdate() {
  update_task_.Reset(base::BindOnce(&XDisplayManager::UpdateDisplayList,
                                    base::Unretained(this)));
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                update_task_.callback());
}

gfx::Point XDisplayManager::GetCursorLocation() const {
  if (auto response = connection_->QueryPointer({x_root_window_}).Sync())
    return {response->root_x, response->root_y};
  return {};
}

std::string XDisplayManager::GetCurrentWorkspace() {
  return workspace_handler_.GetCurrentWorkspace();
}

}  // namespace ui
