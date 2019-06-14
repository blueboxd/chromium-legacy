// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/kiosk_next/kiosk_next_shell_controller_impl.h"

#include <memory>
#include <utility>

#include "ash/home_screen/home_screen_controller.h"
#include "ash/kiosk_next/kiosk_next_home_controller.h"
#include "ash/kiosk_next/kiosk_next_shell_observer.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/metrics/histogram_macros.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

std::unique_ptr<ShelfModel> CreateKioskNextShelfModel() {
  auto shelf_model = std::make_unique<ShelfModel>();

  DCHECK_EQ(0, shelf_model->ItemIndexByID(ShelfID(kBackButtonId)));
  DCHECK_EQ(1, shelf_model->ItemIndexByID(ShelfID(kAppListId)));

  ShelfItem back_item = shelf_model->items()[0];
  ShelfItem home_item = shelf_model->items()[1];

  back_item.title = l10n_util::GetStringUTF16(IDS_ASH_SHELF_BACK_BUTTON_TITLE);
  home_item.title =
      l10n_util::GetStringUTF16(IDS_ASH_SHELF_APP_LIST_LAUNCHER_TITLE);

  shelf_model->Set(0, back_item);
  shelf_model->Set(1, home_item);
  return shelf_model;
}

}  // namespace

KioskNextShellControllerImpl::KioskNextShellControllerImpl() = default;

KioskNextShellControllerImpl::~KioskNextShellControllerImpl() = default;

// static
void KioskNextShellControllerImpl::RegisterProfilePrefs(
    PrefRegistrySimple* registry,
    bool for_test) {
  if (for_test) {
    registry->RegisterBooleanPref(prefs::kKioskNextShellEnabled, false,
                                  PrefRegistry::PUBLIC);
    return;
  }
}

void KioskNextShellControllerImpl::SetClientAndLaunchSession(
    KioskNextShellClient* client) {
  DCHECK_NE(!!client, !!client_);
  client_ = client;
  LaunchKioskNextShellIfEnabled();
}

bool KioskNextShellControllerImpl::IsEnabled() {
  return kiosk_next_enabled_;
}

void KioskNextShellControllerImpl::AddObserver(
    KioskNextShellObserver* observer) {
  observer_list_.AddObserver(observer);
}

void KioskNextShellControllerImpl::RemoveObserver(
    KioskNextShellObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void KioskNextShellControllerImpl::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  LaunchKioskNextShellIfEnabled();
}

void KioskNextShellControllerImpl::LaunchKioskNextShellIfEnabled() {
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  PrefService* pref_service = session_controller->GetPrimaryUserPrefService();
  if (!pref_service)
    return;

  if (!client_)
    return;

  bool prev_kiosk_next_enabled = kiosk_next_enabled_;
  kiosk_next_enabled_ =
      base::FeatureList::IsEnabled(features::kKioskNextShell) &&
      pref_service->GetBoolean(prefs::kKioskNextShellEnabled);
  if (!kiosk_next_enabled_ || prev_kiosk_next_enabled)
    return;

  // Replace the AppListController with a KioskNextHomeController.
  kiosk_next_home_controller_ = std::make_unique<KioskNextHomeController>();
  Shell::Get()->home_screen_controller()->SetDelegate(
      kiosk_next_home_controller_.get());
  Shell::Get()->RemoveAppListController();

  client_->LaunchKioskNextShell(
      session_controller->GetPrimaryUserSession()->user_info.account_id);
  UMA_HISTOGRAM_BOOLEAN("KioskNextShell.Launched", true);

  shelf_model_ = CreateKioskNextShelfModel();

  // Notify observers that KioskNextShell has been enabled.
  for (KioskNextShellObserver& observer : observer_list_) {
    observer.OnKioskNextEnabled();
  }
}

}  // namespace ash
