// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/gesture_education/gesture_education_notification_controller.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "base/bind.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {
const char kNotifierId[] = "ash.gesture_education";
const char kNotificationId[] = "chrome://gesture_education";
}  // namespace

GestureEducationNotificationController::
    GestureEducationNotificationController() {
  Shell::Get()->session_controller()->AddObserver(this);

  chromeos::PowerManagerClient* power_manager_client =
      chromeos::PowerManagerClient::Get();

  power_manager_client->GetSwitchStates(base::BindOnce(
      &GestureEducationNotificationController::OnReceivedSwitchStates,
      weak_ptr_factory_.GetWeakPtr()));
}

GestureEducationNotificationController::
    ~GestureEducationNotificationController() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void GestureEducationNotificationController::OnActiveUserPrefServiceChanged(
    PrefService* prefs) {
  if (!tablet_mode_supported_ ||
      prefs->GetBoolean(prefs::kGestureEducationNotificationShown) ||
      ShelfConfig::Get()->ShelfControlsForcedShownForAccessibility()) {
    return;
  }
  GenerateGestureEducationNotification();
  prefs->SetBoolean(prefs::kGestureEducationNotificationShown, true);
}

void GestureEducationNotificationController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kGestureEducationNotificationShown,
                                false);
}

void GestureEducationNotificationController::
    GenerateGestureEducationNotification() {
  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kNotifierId,
          GetNotificationTitle(), GetNotificationMessage(),
          base::string16() /* display_source */, GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT, kNotificationId),
          message_center::RichNotificationData(),
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(&GestureEducationNotificationController::
                                      HandleNotificationClick,
                                  weak_ptr_factory_.GetWeakPtr())),
          vector_icons::kSettingsIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

void GestureEducationNotificationController::HandleNotificationClick() {
  Shell::Get()->system_tray_model()->client()->ShowGestureEducationHelp();
}

base::string16 GestureEducationNotificationController::GetNotificationMessage()
    const {
  base::string16 system_app_name =
      l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_SYSTEM_APP_NAME);

  base::string16 update_text;
  update_text = l10n_util::GetStringFUTF16(
      IDS_GESTURE_NOTIFICATION_MESSAGE_LEARN_MORE, system_app_name);

  return update_text;
}

base::string16 GestureEducationNotificationController::GetNotificationTitle()
    const {
  return l10n_util::GetStringUTF16(IDS_GESTURE_NOTIFICATION_TITLE);
}

void GestureEducationNotificationController::OnReceivedSwitchStates(
    base::Optional<chromeos::PowerManagerClient::SwitchStates> switch_states) {
  if (switch_states.has_value() &&
      switch_states->tablet_mode !=
          chromeos::PowerManagerClient::TabletMode::UNSUPPORTED) {
    tablet_mode_supported_ = true;
  }
}

}  // namespace ash
