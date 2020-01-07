// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_upgrader.h"

#include "base/barrier_closure.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_manager_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace crostini {

namespace {

class CrostiniUpgraderFactory : public BrowserContextKeyedServiceFactory {
 public:
  static CrostiniUpgrader* GetForProfile(Profile* profile) {
    return static_cast<CrostiniUpgrader*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

  static CrostiniUpgraderFactory* GetInstance() {
    static base::NoDestructor<CrostiniUpgraderFactory> factory;
    return factory.get();
  }

 private:
  friend class base::NoDestructor<CrostiniUpgraderFactory>;

  CrostiniUpgraderFactory()
      : BrowserContextKeyedServiceFactory(
            "CrostiniUpgraderService",
            BrowserContextDependencyManager::GetInstance()) {
    DependsOn(CrostiniManagerFactory::GetInstance());
  }

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    Profile* profile = Profile::FromBrowserContext(context);
    return new CrostiniUpgrader(profile);
  }
};
}  // namespace

CrostiniUpgrader* CrostiniUpgrader::GetForProfile(Profile* profile) {
  return CrostiniUpgraderFactory::GetForProfile(profile);
}

CrostiniUpgrader::CrostiniUpgrader(Profile* profile)
    : profile_(profile), container_id_("", ""), pmc_observer_(this) {
  CrostiniManager::GetForProfile(profile_)->AddUpgradeContainerProgressObserver(
      this);
}

CrostiniUpgrader::~CrostiniUpgrader() = default;

void CrostiniUpgrader::Shutdown() {
  CrostiniManager::GetForProfile(profile_)
      ->RemoveUpgradeContainerProgressObserver(this);
  upgrader_observers_.Clear();
}

void CrostiniUpgrader::AddObserver(CrostiniUpgraderUIObserver* observer) {
  upgrader_observers_.AddObserver(observer);
}

void CrostiniUpgrader::RemoveObserver(CrostiniUpgraderUIObserver* observer) {
  upgrader_observers_.RemoveObserver(observer);
}

void CrostiniUpgrader::Backup() {
  // Just pretend for now. Real import/export code starts here.
  base::PostDelayedTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&CrostiniUpgrader::OnBackup,
                     weak_ptr_factory_.GetWeakPtr(), CrostiniResult::SUCCESS),
      base::TimeDelta::FromSeconds(5));
}

void CrostiniUpgrader::OnBackup(CrostiniResult result) {
  if (result != CrostiniResult::SUCCESS) {
    for (auto& observer : upgrader_observers_) {
      observer.OnBackupFailed();
    }
    return;
  }
  for (auto& observer : upgrader_observers_) {
    observer.OnBackupSucceeded();
  }
}

void CrostiniUpgrader::StartPrechecks() {
  auto* pmc = chromeos::PowerManagerClient::Get();
  if (pmc_observer_.IsObserving(pmc)) {
    // This could happen if two StartPrechecks were run at the same time. If it
    // does, drop the second call.
    return;
  }

  prechecks_callback_ =
      base::BarrierClosure(2, /* Number of asynchronous prechecks to wait for */
                           base::BindOnce(&CrostiniUpgrader::DoPrechecks,
                                          weak_ptr_factory_.GetWeakPtr()));

  pmc_observer_.Add(pmc);
  pmc->RequestStatusUpdate();

  base::PostTaskAndReplyWithResult(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace,
                     base::FilePath(crostini::kHomeDirectory)),
      base::BindOnce(&CrostiniUpgrader::OnAvailableDiskSpace,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniUpgrader::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  // A battery can be FULL, CHARGING, DISCHARGING, or NOT_PRESENT. If we're on a
  // system with no battery, we can assume stable power from the fact that we
  // are running at all. Otherwise we want the battery to be full or charging. A
  // less conservative check is possible, but we can expect users to have access
  // to a charger.
  power_status_good_ = proto.battery_state() !=
                       power_manager::PowerSupplyProperties::DISCHARGING;

  auto* pmc = chromeos::PowerManagerClient::Get();
  pmc_observer_.Remove(pmc);

  prechecks_callback_.Run();
}

void CrostiniUpgrader::OnAvailableDiskSpace(int64_t bytes) {
  free_disk_space_ = bytes;

  prechecks_callback_.Run();
}

void CrostiniUpgrader::DoPrechecks() {
  chromeos::crostini_upgrader::mojom::UpgradePrecheckStatus status;
  if (free_disk_space_ < kDiskRequired) {
    status = chromeos::crostini_upgrader::mojom::UpgradePrecheckStatus::
        INSUFFICIENT_SPACE;
  } else if (content::GetNetworkConnectionTracker()->IsOffline()) {
    status = chromeos::crostini_upgrader::mojom::UpgradePrecheckStatus::
        NETWORK_FAILURE;
  } else if (!power_status_good_) {
    status =
        chromeos::crostini_upgrader::mojom::UpgradePrecheckStatus::LOW_POWER;
  } else {
    status = chromeos::crostini_upgrader::mojom::UpgradePrecheckStatus::OK;
  }

  for (auto& observer : upgrader_observers_) {
    observer.PrecheckStatus(status);
  }
}

void CrostiniUpgrader::Upgrade(const ContainerId& container_id) {
  container_id_ = container_id;
  CrostiniManager::GetForProfile(profile_)->UpgradeContainer(
      container_id_, ContainerVersion::STRETCH, ContainerVersion::BUSTER,
      base::BindOnce(&CrostiniUpgrader::OnUpgrade,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniUpgrader::OnUpgrade(CrostiniResult result) {
  if (result != CrostiniResult::SUCCESS) {
    LOG(ERROR) << "OnUpgrade result " << static_cast<int>(result);
    for (auto& observer : upgrader_observers_) {
      observer.OnUpgradeFailed();
    }
    return;
  }
}

void CrostiniUpgrader::Cancel() {
  CrostiniManager::GetForProfile(profile_)->CancelUpgradeContainer(
      container_id_, base::BindOnce(&CrostiniUpgrader::OnCancel,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniUpgrader::OnCancel(CrostiniResult result) {
  for (auto& observer : upgrader_observers_) {
    observer.OnCanceled();
  }
}

void CrostiniUpgrader::CancelBeforeStart() {
  for (auto& observer : upgrader_observers_) {
    observer.OnCanceled();
  }
}

void CrostiniUpgrader::OnUpgradeContainerProgress(
    const ContainerId& container_id,
    UpgradeContainerProgressStatus status,
    const std::vector<std::string>& messages) {
  if (container_id != container_id_) {
    return;
  }
  switch (status) {
    case UpgradeContainerProgressStatus::UPGRADING:
      for (auto& observer : upgrader_observers_) {
        observer.OnUpgradeProgress(messages);
      }
      break;
    case UpgradeContainerProgressStatus::SUCCEEDED:
      for (auto& observer : upgrader_observers_) {
        observer.OnUpgradeSucceeded();
      }
      break;
    case UpgradeContainerProgressStatus::FAILED:
      for (auto& observer : upgrader_observers_) {
        observer.OnUpgradeFailed();
      }
      break;
  }
}

// Return true if internal state allows starting upgrade.
bool CrostiniUpgrader::CanUpgrade() {
  return false;
}

}  // namespace crostini
