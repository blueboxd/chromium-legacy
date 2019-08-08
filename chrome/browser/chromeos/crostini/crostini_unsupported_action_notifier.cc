// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_unsupported_action_notifier.h"

#include <utility>

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/toast_manager.h"
#include "base/logging.h"
#include "base/optional.h"
#include "chrome/grit/generated_resources.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace crostini {

CrostiniUnsupportedActionNotifier::CrostiniUnsupportedActionNotifier()
    : CrostiniUnsupportedActionNotifier(std::make_unique<Delegate>()) {}

CrostiniUnsupportedActionNotifier::CrostiniUnsupportedActionNotifier(
    std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {
  delegate_->AddTabletModeObserver(this);
  delegate_->AddFocusObserver(this);
}

CrostiniUnsupportedActionNotifier::~CrostiniUnsupportedActionNotifier() {
  delegate_->RemoveTabletModeObserver(this);
  delegate_->RemoveFocusObserver(this);
}

void CrostiniUnsupportedActionNotifier::
    ShowVirtualKeyboardUnsupportedNotifictionIfNeeded() {
  if (!virtual_keyboard_unsupported_message_shown_ &&
      delegate_->IsInTabletMode() && delegate_->IsFocusedWindowCrostini()) {
    ash::ToastData data = {
        /*id=*/"VKUnsupportedInCrostini",
        /*text=*/
        l10n_util::GetStringUTF16(IDS_CROSTINI_UNSUPPORTED_VIRTUAL_KEYBOARD),
        /*timeout_ms=*/5000,
        /*dismiss_text=*/base::Optional<base::string16>()};
    delegate_->ShowToast(data);
    virtual_keyboard_unsupported_message_shown_ = true;
  }
}

void CrostiniUnsupportedActionNotifier::OnTabletModeStarted() {
  ShowVirtualKeyboardUnsupportedNotifictionIfNeeded();
}

void CrostiniUnsupportedActionNotifier::OnWindowFocused(
    aura::Window* gained_focus,
    aura::Window* lost_focus) {
  ShowVirtualKeyboardUnsupportedNotifictionIfNeeded();
}

CrostiniUnsupportedActionNotifier::Delegate::Delegate() = default;

CrostiniUnsupportedActionNotifier::Delegate::~Delegate() = default;

bool CrostiniUnsupportedActionNotifier::Delegate::IsInTabletMode() {
  return ash::TabletMode::Get()->InTabletMode();
}

bool CrostiniUnsupportedActionNotifier::Delegate::IsFocusedWindowCrostini() {
  if (!exo::WMHelper::HasInstance()) {
    return false;
  }
  auto* focused_window = exo::WMHelper::GetInstance()->GetFocusedWindow();
  return focused_window->GetProperty(aura::client::kAppType) ==
         static_cast<int>(ash::AppType::CROSTINI_APP);
}

void CrostiniUnsupportedActionNotifier::Delegate::ShowToast(
    const ash::ToastData& toast_data) {
  ash::ToastManager::Get()->Show(toast_data);
}

void CrostiniUnsupportedActionNotifier::Delegate::AddFocusObserver(
    aura::client::FocusChangeObserver* observer) {
  if (exo::WMHelper::HasInstance()) {
    exo::WMHelper::GetInstance()->AddFocusObserver(observer);
  }
}

void CrostiniUnsupportedActionNotifier::Delegate::RemoveFocusObserver(
    aura::client::FocusChangeObserver* observer) {
  if (exo::WMHelper::HasInstance()) {
    exo::WMHelper::GetInstance()->RemoveFocusObserver(observer);
  }
}

void CrostiniUnsupportedActionNotifier::Delegate::AddTabletModeObserver(
    TabletModeObserver* observer) {
  auto* client = ash::TabletMode::Get();
  DCHECK(client);
  client->AddObserver(observer);
}

void CrostiniUnsupportedActionNotifier::Delegate::RemoveTabletModeObserver(
    TabletModeObserver* observer) {
  auto* client = ash::TabletMode::Get();
  DCHECK(client);
  client->RemoveObserver(observer);
}

}  // namespace crostini
