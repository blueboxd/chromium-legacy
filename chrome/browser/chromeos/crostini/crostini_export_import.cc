// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_export_import.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/crostini/crostini_manager_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_share_path.h"
#include "chrome/browser/chromeos/crostini/crostini_share_path_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

namespace crostini {

class CrostiniExportImportFactory : public BrowserContextKeyedServiceFactory {
 public:
  static CrostiniExportImport* GetForProfile(Profile* profile) {
    return static_cast<CrostiniExportImport*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

  static CrostiniExportImportFactory* GetInstance() {
    static base::NoDestructor<CrostiniExportImportFactory> factory;
    return factory.get();
  }

 private:
  friend class base::NoDestructor<CrostiniExportImportFactory>;

  CrostiniExportImportFactory()
      : BrowserContextKeyedServiceFactory(
            "CrostiniExportImportService",
            BrowserContextDependencyManager::GetInstance()) {
    DependsOn(CrostiniSharePathFactory::GetInstance());
    DependsOn(CrostiniManagerFactory::GetInstance());
  }

  ~CrostiniExportImportFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    Profile* profile = Profile::FromBrowserContext(context);
    return new CrostiniExportImport(profile);
  }
};

CrostiniExportImport* CrostiniExportImport::GetForProfile(Profile* profile) {
  return CrostiniExportImportFactory::GetForProfile(profile);
}

CrostiniExportImport::CrostiniExportImport(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {
  CrostiniManager* manager = CrostiniManager::GetForProfile(profile_);
  manager->AddExportContainerProgressObserver(this);
  manager->AddImportContainerProgressObserver(this);
}

CrostiniExportImport::~CrostiniExportImport() = default;

void CrostiniExportImport::Shutdown() {
  CrostiniManager* manager = CrostiniManager::GetForProfile(profile_);
  manager->RemoveExportContainerProgressObserver(this);
  manager->RemoveImportContainerProgressObserver(this);
}

void CrostiniExportImport::ExportContainer(content::WebContents* web_contents) {
  if (!IsCrostiniExportImportUIAllowedForProfile(profile_)) {
    return;
  }
  OpenFileDialog(ExportImportType::EXPORT, web_contents);
}

void CrostiniExportImport::ImportContainer(content::WebContents* web_contents) {
  if (!IsCrostiniExportImportUIAllowedForProfile(profile_)) {
    return;
  }
  OpenFileDialog(ExportImportType::IMPORT, web_contents);
}

void CrostiniExportImport::OpenFileDialog(ExportImportType type,
                                          content::WebContents* web_contents) {
  PrefService* pref_service = profile_->GetPrefs();
  ui::SelectFileDialog::Type file_selector_mode;
  unsigned title = 0;
  base::FilePath default_path;
  ui::SelectFileDialog::FileTypeInfo file_types;
  file_types.allowed_paths =
      ui::SelectFileDialog::FileTypeInfo::NATIVE_OR_DRIVE_PATH;
  file_types.extensions = {{"tar.gz", "tgz"}};

  switch (type) {
    case ExportImportType::EXPORT:
      file_selector_mode = ui::SelectFileDialog::SELECT_SAVEAS_FILE;
      title = IDS_SETTINGS_CROSTINI_EXPORT;
      base::Time::Exploded exploded;
      base::Time::Now().LocalExplode(&exploded);
      default_path =
          pref_service->GetFilePath(prefs::kDownloadDefaultDirectory)
              .Append(base::StringPrintf("chromeos-linux-%04d-%02d-%02d.tar.gz",
                                         exploded.year, exploded.month,
                                         exploded.day_of_month));
      break;
    case ExportImportType::IMPORT:
      file_selector_mode = ui::SelectFileDialog::SELECT_OPEN_FILE,
      title = IDS_SETTINGS_CROSTINI_IMPORT;
      default_path =
          pref_service->GetFilePath(prefs::kDownloadDefaultDirectory);
      break;
  }

  select_folder_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));
  select_folder_dialog_->SelectFile(
      file_selector_mode, l10n_util::GetStringUTF16(title), default_path,
      &file_types, 0, base::FilePath::StringType(),
      web_contents->GetTopLevelNativeWindow(), reinterpret_cast<void*>(type));
}

void CrostiniExportImport::FileSelected(const base::FilePath& path,
                                        int index,
                                        void* params) {
  ExportImportType type =
      static_cast<ExportImportType>(reinterpret_cast<uintptr_t>(params));
  Start(type,
        ContainerId(kCrostiniDefaultVmName, kCrostiniDefaultContainerName),
        path, base::DoNothing());
}

void CrostiniExportImport::ExportContainer(
    ContainerId container_id,
    base::FilePath path,
    CrostiniManager::CrostiniResultCallback callback) {
  Start(ExportImportType::EXPORT, container_id, path, std::move(callback));
}

void CrostiniExportImport::ImportContainer(
    ContainerId container_id,
    base::FilePath path,
    CrostiniManager::CrostiniResultCallback callback) {
  Start(ExportImportType::IMPORT, container_id, path, std::move(callback));
}

void CrostiniExportImport::Start(
    ExportImportType type,
    const ContainerId& container_id,
    base::FilePath path,
    CrostiniManager::CrostiniResultCallback callback) {
  notifications_[container_id] =
      std::make_unique<CrostiniExportImportNotification>(
          profile_, this, type, GetUniqueNotificationId(), path);

  switch (type) {
    case ExportImportType::EXPORT:
      CrostiniSharePath::GetForProfile(profile_)->SharePath(
          kCrostiniDefaultVmName, path.DirName(), false,
          base::BindOnce(&CrostiniExportImport::ExportAfterSharing,
                         weak_ptr_factory_.GetWeakPtr(), container_id,
                         path.BaseName(), std::move(callback)));
      break;
    case ExportImportType::IMPORT:
      CrostiniSharePath::GetForProfile(profile_)->SharePath(
          kCrostiniDefaultVmName, path, false,
          base::BindOnce(&CrostiniExportImport::ImportAfterSharing,
                         weak_ptr_factory_.GetWeakPtr(), container_id,
                         std::move(callback)));
      break;
  }
}

void CrostiniExportImport::ExportAfterSharing(
    const ContainerId& container_id,
    const base::FilePath& filename,
    CrostiniManager::CrostiniResultCallback callback,
    const base::FilePath& container_path,
    bool result,
    const std::string failure_reason) {
  if (!result) {
    LOG(ERROR) << "Error sharing for export " << container_path.value() << ": "
               << failure_reason;
    auto it = notifications_.find(container_id);
    DCHECK(it != notifications_.end()) << ContainerIdToString(container_id)
                                       << " has no notification to update";
    it->second->UpdateStatus(CrostiniExportImportNotification::Status::FAILED,
                             0);
    return;
  }
  CrostiniManager::GetForProfile(profile_)->ExportLxdContainer(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      container_path.Append(filename),
      base::BindOnce(&CrostiniExportImport::OnExportComplete,
                     weak_ptr_factory_.GetWeakPtr(), base::Time::Now(),
                     container_id, std::move(callback)));
}

void CrostiniExportImport::OnExportComplete(
    const base::Time& start,
    const ContainerId& container_id,
    CrostiniManager::CrostiniResultCallback callback,
    CrostiniResult result) {
  auto it = notifications_.find(container_id);
  DCHECK(it != notifications_.end())
      << ContainerIdToString(container_id) << " has no notification to update";

  CrostiniExportImportNotification::Status status =
      CrostiniExportImportNotification::Status::DONE;
  ExportContainerResult enum_hist_result = ExportContainerResult::kSuccess;
  if (result == crostini::CrostiniResult::SUCCESS) {
    UMA_HISTOGRAM_LONG_TIMES("Crostini.BackupTimeSuccess",
                             base::Time::Now() - start);
  } else {
    LOG(ERROR) << "Error exporting " << int(result);
    status = CrostiniExportImportNotification::Status::FAILED;
    switch (result) {
      case crostini::CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_VM_STOPPED:
        enum_hist_result = ExportContainerResult::kFailedVmStopped;
        break;
      case crostini::CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_VM_STARTED:
        enum_hist_result = ExportContainerResult::kFailedVmStarted;
        break;
      default:
        enum_hist_result = ExportContainerResult::kFailed;
    }
    UMA_HISTOGRAM_LONG_TIMES("Crostini.BackupTimeFailed",
                             base::Time::Now() - start);
  }
  it->second->UpdateStatus(status, 0);
  UMA_HISTOGRAM_ENUMERATION("Crostini.Backup", enum_hist_result);
  std::move(callback).Run(result);
}

void CrostiniExportImport::OnExportContainerProgress(
    const std::string& vm_name,
    const std::string& container_name,
    ExportContainerProgressStatus status,
    int progress_percent,
    uint64_t progress_speed) {
  ContainerId container_id(vm_name, container_name);
  auto it = notifications_.find(container_id);
  DCHECK(it != notifications_.end())
      << ContainerIdToString(container_id) << " has no notification to update";

  switch (status) {
    // Rescale PACK:1-100 => 0-50.
    case ExportContainerProgressStatus::PACK:
      it->second->UpdateStatus(
          CrostiniExportImportNotification::Status::RUNNING,
          progress_percent / 2);
      break;
    // Rescale DOWNLOAD:1-100 => 50-100.
    case ExportContainerProgressStatus::DOWNLOAD:
      it->second->UpdateStatus(
          CrostiniExportImportNotification::Status::RUNNING,
          50 + progress_percent / 2);
      break;
    default:
      LOG(WARNING) << "Unknown Export progress status " << int(status);
  }
}

void CrostiniExportImport::ImportAfterSharing(
    const ContainerId& container_id,
    CrostiniManager::CrostiniResultCallback callback,
    const base::FilePath& container_path,
    bool result,
    const std::string failure_reason) {
  if (!result) {
    LOG(ERROR) << "Error sharing for import " << container_path.value() << ": "
               << failure_reason;
    auto it = notifications_.find(container_id);
    DCHECK(it != notifications_.end()) << ContainerIdToString(container_id)
                                       << " has no notification to update";
    it->second->UpdateStatus(CrostiniExportImportNotification::Status::FAILED,
                             0);
    return;
  }
  CrostiniManager::GetForProfile(profile_)->ImportLxdContainer(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName, container_path,
      base::BindOnce(&CrostiniExportImport::OnImportComplete,
                     weak_ptr_factory_.GetWeakPtr(), base::Time::Now(),
                     container_id, std::move(callback)));
}

void CrostiniExportImport::OnImportComplete(
    const base::Time& start,
    const ContainerId& container_id,
    CrostiniManager::CrostiniResultCallback callback,
    CrostiniResult result) {
  auto it = notifications_.find(container_id);
  DCHECK(it != notifications_.end())
      << ContainerIdToString(container_id) << " has no notification to update";
  CrostiniExportImportNotification::Status status =
      CrostiniExportImportNotification::Status::DONE;
  ImportContainerResult enum_hist_result = ImportContainerResult::kSuccess;
  if (result == crostini::CrostiniResult::SUCCESS) {
    UMA_HISTOGRAM_LONG_TIMES("Crostini.RestoreTimeSuccess",
                             base::Time::Now() - start);
  } else {
    LOG(ERROR) << "Error importing " << int(result);
    status = CrostiniExportImportNotification::Status::FAILED;
    switch (result) {
      case crostini::CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_VM_STOPPED:
        enum_hist_result = ImportContainerResult::kFailedVmStopped;
        break;
      case crostini::CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_VM_STARTED:
        enum_hist_result = ImportContainerResult::kFailedVmStarted;
        break;
      case crostini::CrostiniResult::
          CONTAINER_EXPORT_IMPORT_FAILED_ARCHITECTURE:
        enum_hist_result = ImportContainerResult::kFailedArchitecture;
        break;
      case crostini::CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_SPACE:
        enum_hist_result = ImportContainerResult::kFailedSpace;
        break;
      default:
        enum_hist_result = ImportContainerResult::kFailed;
    }
    UMA_HISTOGRAM_LONG_TIMES("Crostini.RestoreTimeFailed",
                             base::Time::Now() - start);
  }
  it->second->UpdateStatus(status, 0);
  UMA_HISTOGRAM_ENUMERATION("Crostini.Restore", enum_hist_result);

  // Restart from CrostiniManager.
  CrostiniManager::GetForProfile(profile_)->RestartCrostini(
      container_id.first, container_id.second, std::move(callback));
}

void CrostiniExportImport::OnImportContainerProgress(
    const std::string& vm_name,
    const std::string& container_name,
    ImportContainerProgressStatus status,
    int progress_percent,
    uint64_t progress_speed,
    const std::string& architecture_device,
    const std::string& architecture_container,
    uint64_t available_space,
    uint64_t minimum_required_space) {
  ContainerId container_id(vm_name, container_name);
  auto it = notifications_.find(container_id);
  DCHECK(it != notifications_.end())
      << ContainerIdToString(container_id) << " has no notification to update";

  switch (status) {
    // Rescale UPLOAD:1-100 => 0-50.
    case ImportContainerProgressStatus::UPLOAD:
      it->second->UpdateStatus(
          CrostiniExportImportNotification::Status::RUNNING,
          progress_percent / 2);
      break;
    // Rescale UNPACK:1-100 => 50-100.
    case ImportContainerProgressStatus::UNPACK:
      it->second->UpdateStatus(
          CrostiniExportImportNotification::Status::RUNNING,
          50 + progress_percent / 2);
      break;
    // Failure, set error message.
    case ImportContainerProgressStatus::FAILURE_ARCHITECTURE:
      it->second->set_message_failed(l10n_util::GetStringFUTF16(
          IDS_CROSTINI_IMPORT_NOTIFICATION_MESSAGE_FAILED_ARCHITECTURE,
          base::ASCIIToUTF16(architecture_container),
          base::ASCIIToUTF16(architecture_device)));
      break;
    case ImportContainerProgressStatus::FAILURE_SPACE:
      DCHECK_GE(minimum_required_space, available_space);
      it->second->set_message_failed(l10n_util::GetStringFUTF16(
          IDS_CROSTINI_IMPORT_NOTIFICATION_MESSAGE_FAILED_SPACE,
          ui::FormatBytes(minimum_required_space - available_space)));
      break;
    default:
      LOG(WARNING) << "Unknown Export progress status " << int(status);
  }
}

std::string CrostiniExportImport::GetUniqueNotificationId() {
  return base::StringPrintf("crostini_export_import_%d",
                            next_notification_id_++);
}

void CrostiniExportImport::NotificationCompleted(
    CrostiniExportImportNotification* notification) {
  for (auto it = notifications_.begin(); it != notifications_.end(); ++it) {
    if (it->second.get() == notification) {
      notifications_.erase(it);
      return;
    }
  }
  // Notification should always exist when this is called.
  NOTREACHED();
}

CrostiniExportImportNotification*
CrostiniExportImport::GetNotificationForTesting(ContainerId container_id) {
  auto it = notifications_.find(container_id);
  return it != notifications_.end() ? it->second.get() : nullptr;
}

}  // namespace crostini
