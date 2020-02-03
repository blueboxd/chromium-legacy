// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/installed_version_updater_chromeos.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace {

// The reason of the rollback used in the UpgradeDetector.RollbackReason
// histogram.
enum class RollbackReason {
  kToMoreStableChannel = 0,
  kEnterpriseRollback = 1,
  kMaxValue = kEnterpriseRollback,
};

}  // namespace

InstalledVersionUpdater::InstalledVersionUpdater(BuildState* build_state)
    : build_state_(build_state) {
  chromeos::DBusThreadManager::Get()->GetUpdateEngineClient()->AddObserver(
      this);
}

InstalledVersionUpdater::~InstalledVersionUpdater() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  chromeos::DBusThreadManager::Get()->GetUpdateEngineClient()->RemoveObserver(
      this);
}

void InstalledVersionUpdater::UpdateStatusChanged(
    const update_engine::StatusResult& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status.current_operation() !=
      update_engine::Operation::UPDATED_NEED_REBOOT) {
    return;
  }

  // Cancel a potential channel request from a previous status update.
  weak_ptr_factory_.InvalidateWeakPtrs();
  current_channel_.reset();
  target_channel_.reset();

  if (status.is_enterprise_rollback()) {
    // Powerwash will be required. Determine what kind of notification to show
    // based on the current and target channels.
    // StatusResult::new_version is the new Chrome OS system version number.
    new_version_ = status.new_version();
    auto* client = chromeos::DBusThreadManager::Get()->GetUpdateEngineClient();
    client->GetChannel(/*get_current_channel=*/false,
                       base::BindOnce(&InstalledVersionUpdater::OnChannel,
                                      weak_ptr_factory_.GetWeakPtr(), false));
    client->GetChannel(/*get_current_channel=*/true,
                       base::BindOnce(&InstalledVersionUpdater::OnChannel,
                                      weak_ptr_factory_.GetWeakPtr(), true));
  } else {
    // Not going to an earlier version; no powerwash or rollback message is
    // required.
    new_version_.reset();
    build_state_->SetUpdate(BuildState::UpdateType::kNormalUpdate,
                            base::Version(status.new_version()), base::nullopt);
  }
}

void InstalledVersionUpdater::OnChannel(bool is_current_channel,
                                        const std::string& channel_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Preserve this channel name.
  (is_current_channel ? current_channel_ : target_channel_) = channel_name;

  // Nothing else to do if still waiting on the other value.
  if (!current_channel_.has_value() || !target_channel_.has_value())
    return;

  // Set the UpdateType based on whether or not the device is moving to a more
  // stable channel.
  // TODO(crbug.com/864672): Use is_rollback from the update engine so that
  // admin-driven channel changes appear as enterprise rollbacks rather than
  // channel switches.
  BuildState::UpdateType update_type =
      chromeos::UpdateEngineClient::IsTargetChannelMoreStable(
          current_channel_.value(), target_channel_.value())
          ? BuildState::UpdateType::kChannelSwitchRollback
          : BuildState::UpdateType::kEnterpriseRollback;

  base::UmaHistogramEnumeration(
      "UpgradeDetector.RollbackReason",
      update_type == BuildState::UpdateType::kChannelSwitchRollback
          ? RollbackReason::kToMoreStableChannel
          : RollbackReason::kEnterpriseRollback);

  LOG(WARNING) << "Device is rolling back, will require powerwash. Reason: "
               << (update_type ==
                   BuildState::UpdateType::kChannelSwitchRollback)
               << ", current_channel: " << current_channel_.value()
               << ", target_channel: " << target_channel_.value();

  build_state_->SetUpdate(update_type, base::Version(new_version_.value()),
                          base::nullopt);

  // All done; clear state.
  new_version_.reset();
  current_channel_.reset();
  target_channel_.reset();
}
