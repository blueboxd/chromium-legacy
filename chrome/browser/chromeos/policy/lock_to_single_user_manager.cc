// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/lock_to_single_user_manager.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/login/session/session_termination_manager.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "content/public/browser/notification_service.h"

using RebootOnSignOutPolicy =
    enterprise_management::DeviceRebootOnUserSignoutProto;
using RebootOnSignOutRequest =
    cryptohome::LockToSingleUserMountUntilRebootRequest;
using RebootOnSignOutReply = cryptohome::LockToSingleUserMountUntilRebootReply;
using RebootOnSignOutResult =
    cryptohome::LockToSingleUserMountUntilRebootResult;

namespace policy {

namespace {

// The result from D-Bus call to lock the device to single user.
// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "LockToSingleUserResult" in src/tools/metrics/histograms/enums.xml.
enum class LockToSingleUserResult {
  // Successfully locked to single user.
  kSuccess = 0,
  // No response from DBus call.
  kNoResponse = 1,
  // Request failed on Chrome OS side.
  kFailedToLock = 2,
  kMaxValue = kFailedToLock,
};

void RecordDBusResult(LockToSingleUserResult result) {
  base::UmaHistogramEnumeration("Enterprise.LockToSingleUserResult", result);
}

}  // namespace

LockToSingleUserManager::LockToSingleUserManager() {
  notification_registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CREATED,
                              content::NotificationService::AllSources());
}

LockToSingleUserManager::~LockToSingleUserManager() = default;

void LockToSingleUserManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(chrome::NOTIFICATION_PROFILE_CREATED, type);
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(
          content::Source<Profile>(source).ptr());
  if (!user || user->IsAffiliated()) {
    return;
  }
  int policy_value = -1;
  if (!chromeos::CrosSettings::Get()->GetInteger(
          chromeos::kDeviceRebootOnUserSignout, &policy_value)) {
    return;
  }
  notification_registrar_.RemoveAll();
  switch (policy_value) {
    case RebootOnSignOutPolicy::ALWAYS:
      LockToSingleUser();
      break;
    case RebootOnSignOutPolicy::ARC_SESSION:
      arc_session_observer_.Add(arc::ArcSessionManager::Get());
      break;
  }
}

void LockToSingleUserManager::OnArcStarted() {
  arc_session_observer_.RemoveAll();
  LockToSingleUser();
}

void LockToSingleUserManager::LockToSingleUser() {
  cryptohome::AccountIdentifier account_id =
      cryptohome::CreateAccountIdentifierFromAccountId(
          user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId());
  RebootOnSignOutRequest request;
  request.mutable_account_id()->CopyFrom(account_id);
  chromeos::CryptohomeClient::Get()->LockToSingleUserMountUntilReboot(
      request,
      base::BindOnce(
          &LockToSingleUserManager::OnLockToSingleUserMountUntilRebootDone,
          weak_factory_.GetWeakPtr()));
}

void LockToSingleUserManager::OnLockToSingleUserMountUntilRebootDone(
    base::Optional<cryptohome::BaseReply> reply) {
  if (!reply || !reply->HasExtension(RebootOnSignOutReply::reply)) {
    LOG(ERROR) << "Signing out user: no reply from "
                  "LockToSingleUserMountUntilReboot D-Bus call.";
    RecordDBusResult(LockToSingleUserResult::kNoResponse);
    chrome::AttemptUserExit();
    return;
  }

  // Force user logout if failed to lock the device to single user mount.
  const RebootOnSignOutReply extension =
      reply->GetExtension(RebootOnSignOutReply::reply);
  if (extension.result() == RebootOnSignOutResult::SUCCESS ||
      extension.result() == RebootOnSignOutResult::PCR_ALREADY_EXTENDED) {
    // The device is locked to single user on TPM level. Update the cache in
    // SessionTerminationManager, so that it triggers reboot on sign out.
    chromeos::SessionTerminationManager::Get()->SetDeviceLockedToSingleUser();
    RecordDBusResult(LockToSingleUserResult::kSuccess);
  } else {
    LOG(ERROR) << "Signing out user: failed to lock device to single user: "
               << extension.result();
    RecordDBusResult(LockToSingleUserResult::kFailedToLock);
    chrome::AttemptUserExit();
  }
}

}  // namespace policy
