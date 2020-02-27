// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/device_storage_handler.h"

#include <algorithm>
#include <limits>
#include <string>

#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/disks/disk.h"
#include "components/arc/arc_features.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

using chromeos::disks::Disk;
using chromeos::disks::DiskMountManager;

namespace chromeos {
namespace settings {


namespace {

constexpr char kAndroidEnabled[] = "androidEnabled";

}  // namespace

StorageHandler::StorageHandler(Profile* profile,
                               content::WebUIDataSource* html_source)
    : size_stat_calculator_("storage-size-stat-changed", profile),
      my_files_size_calculator_("storage-my-files-size-changed", profile),
      browsing_data_size_calculator_("storage-browsing-data-size-changed",
                                     profile),
      apps_size_calculator_("storage-apps-size-changed", profile),
      crostini_size_calculator_("storage-crostini-size-changed", profile),
      other_users_size_calculator_("storage-other-users-size-changed"),
      profile_(profile),
      source_name_(html_source->GetSource()),
      arc_observer_(this),
      special_volume_path_pattern_("[a-z]+://.*") {
  html_source->AddBoolean(
      kAndroidEnabled,
      base::FeatureList::IsEnabled(arc::kUsbStorageUIFeature) &&
          arc::IsArcPlayStoreEnabledForProfile(profile));
}

StorageHandler::~StorageHandler() {
  StopObservingEvents();
}

void StorageHandler::RegisterMessages() {
  DCHECK(web_ui());

  web_ui()->RegisterMessageCallback(
      "updateAndroidEnabled",
      base::BindRepeating(&StorageHandler::HandleUpdateAndroidEnabled,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "updateStorageInfo",
      base::BindRepeating(&StorageHandler::HandleUpdateStorageInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openMyFiles", base::BindRepeating(&StorageHandler::HandleOpenMyFiles,
                                         base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openArcStorage",
      base::BindRepeating(&StorageHandler::HandleOpenArcStorage,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "updateExternalStorages",
      base::BindRepeating(&StorageHandler::HandleUpdateExternalStorages,
                          base::Unretained(this)));
}

void StorageHandler::OnJavascriptAllowed() {
  if (base::FeatureList::IsEnabled(arc::kUsbStorageUIFeature))
    arc_observer_.Add(arc::ArcSessionManager::Get());

  // Start observing mount/unmount events to update the connected device list.
  DiskMountManager::GetInstance()->AddObserver(this);

  // Start observing calculators.
  size_stat_calculator_.AddObserver(this);
  my_files_size_calculator_.AddObserver(this);
  browsing_data_size_calculator_.AddObserver(this);
  apps_size_calculator_.AddObserver(this);
  crostini_size_calculator_.AddObserver(this);
  other_users_size_calculator_.AddObserver(this);
}

void StorageHandler::OnJavascriptDisallowed() {
  // Ensure that pending callbacks do not complete and cause JS to be evaluated.
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (base::FeatureList::IsEnabled(arc::kUsbStorageUIFeature))
    arc_observer_.Remove(arc::ArcSessionManager::Get());

  StopObservingEvents();
}

void StorageHandler::HandleUpdateAndroidEnabled(
    const base::ListValue* unused_args) {
  // OnJavascriptAllowed() calls ArcSessionManager::AddObserver() later.
  AllowJavascript();
}

void StorageHandler::HandleUpdateStorageInfo(const base::ListValue* args) {
  AllowJavascript();

  size_stat_calculator_.StartCalculation();
  my_files_size_calculator_.StartCalculation();
  browsing_data_size_calculator_.StartCalculation();
  apps_size_calculator_.StartCalculation();
  crostini_size_calculator_.StartCalculation();
  other_users_size_calculator_.StartCalculation();
}

void StorageHandler::HandleOpenMyFiles(const base::ListValue* unused_args) {
  const base::FilePath my_files_path =
      file_manager::util::GetMyFilesFolderForProfile(profile_);
  platform_util::OpenItem(profile_, my_files_path, platform_util::OPEN_FOLDER,
                          platform_util::OpenOperationCallback());
}

void StorageHandler::HandleOpenArcStorage(
    const base::ListValue* unused_args) {
  auto* arc_storage_manager =
      arc::ArcStorageManager::GetForBrowserContext(profile_);
  if (arc_storage_manager)
    arc_storage_manager->OpenPrivateVolumeSettings();
}

void StorageHandler::HandleUpdateExternalStorages(
    const base::ListValue* unused_args) {
  UpdateExternalStorages();
}

void StorageHandler::UpdateExternalStorages() {
  base::Value devices(base::Value::Type::LIST);
  for (const auto& itr : DiskMountManager::GetInstance()->mount_points()) {
    const DiskMountManager::MountPointInfo& mount_info = itr.second;
    if (!IsEligibleForAndroidStorage(mount_info.source_path))
      continue;

    const chromeos::disks::Disk* disk =
        DiskMountManager::GetInstance()->FindDiskBySourcePath(
            mount_info.source_path);
    if (!disk)
      continue;

    std::string label = disk->device_label();
    if (label.empty()) {
      // To make volume labels consistent with Files app, we follow how Files
      // generates a volume label when the volume doesn't have specific label.
      // That is, we use the base name of mount path instead in such cases.
      // TODO(fukino): Share the implementation to compute the volume name with
      // Files app. crbug.com/1002535.
      label = base::FilePath(mount_info.mount_path).BaseName().AsUTF8Unsafe();
    }
    base::Value device(base::Value::Type::DICTIONARY);
    device.SetKey("uuid", base::Value(disk->fs_uuid()));
    device.SetKey("label", base::Value(label));
    devices.Append(std::move(device));
  }
  FireWebUIListener("onExternalStoragesUpdated", devices);
}

void StorageHandler::OnArcPlayStoreEnabledChanged(bool enabled) {
  auto update = std::make_unique<base::DictionaryValue>();
  update->SetKey(kAndroidEnabled, base::Value(enabled));
  content::WebUIDataSource::Update(profile_, source_name_, std::move(update));
}

void StorageHandler::OnMountEvent(
    DiskMountManager::MountEvent event,
    chromeos::MountError error_code,
    const DiskMountManager::MountPointInfo& mount_info) {
  if (error_code != chromeos::MountError::MOUNT_ERROR_NONE)
    return;

  if (!IsEligibleForAndroidStorage(mount_info.source_path))
    return;

  UpdateExternalStorages();
}

void StorageHandler::OnSizeCalculated(
    const std::string& event_name,
    int64_t total_bytes,
    const base::Optional<int64_t>& available_bytes) {
  if (available_bytes) {
    UpdateSizeStat(event_name, total_bytes, available_bytes.value());
  } else {
    UpdateStorageItem(event_name, total_bytes);
  }
}

void StorageHandler::StopObservingEvents() {
  // Stop observing mount/unmount events to update the connected device list.
  DiskMountManager::GetInstance()->RemoveObserver(this);

  // Stop observing calculators.
  size_stat_calculator_.RemoveObserver(this);
  my_files_size_calculator_.RemoveObserver(this);
  browsing_data_size_calculator_.RemoveObserver(this);
  apps_size_calculator_.RemoveObserver(this);
  crostini_size_calculator_.RemoveObserver(this);
  other_users_size_calculator_.RemoveObserver(this);
}

void StorageHandler::UpdateStorageItem(const std::string& event_name,
                                       int64_t total_bytes) {
  base::string16 message;
  if (total_bytes < 0) {
    message = l10n_util::GetStringUTF16(IDS_SETTINGS_STORAGE_SIZE_UNKNOWN);
  } else {
    message = ui::FormatBytes(total_bytes);
  }

  FireWebUIListener(event_name, base::Value(message));
}

void StorageHandler::UpdateSizeStat(const std::string& event_name,
                                    int64_t total_bytes,
                                    int64_t available_bytes) {
  int64_t in_use_total_bytes_ = total_bytes - available_bytes;

  base::DictionaryValue size_stat;
  size_stat.SetString("availableSize", ui::FormatBytes(available_bytes));
  size_stat.SetString("usedSize", ui::FormatBytes(in_use_total_bytes_));
  size_stat.SetDouble("usedRatio",
                      static_cast<double>(in_use_total_bytes_) / total_bytes);
  int storage_space_state =
      static_cast<int>(StorageSpaceState::kStorageSpaceNormal);
  if (available_bytes < kSpaceCriticallyLowBytes)
    storage_space_state =
        static_cast<int>(StorageSpaceState::kStorageSpaceCriticallyLow);
  else if (available_bytes < kSpaceLowBytes)
    storage_space_state = static_cast<int>(StorageSpaceState::kStorageSpaceLow);
  size_stat.SetInteger("spaceState", storage_space_state);

  FireWebUIListener(event_name, size_stat);
}

bool StorageHandler::IsEligibleForAndroidStorage(std::string source_path) {
  // Android's StorageManager volume concept relies on assumption that it is
  // local filesystem. Hence, special volumes like DriveFS should not be
  // listed on the Settings.
  return !RE2::FullMatch(source_path, special_volume_path_pattern_);
}

}  // namespace settings
}  // namespace chromeos
