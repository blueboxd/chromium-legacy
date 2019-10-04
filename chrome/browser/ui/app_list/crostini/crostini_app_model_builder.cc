// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/crostini/crostini_app_model_builder.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/crostini/crostini_app_item.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/l10n/l10n_util.h"

// Folder items are created by the Ash process and their existence is
// communicated to chrome via the AppListClient. Therefore, crostini has an
// observer that listens for the creation of its folder, and updates the
// properties accordingly.
class CrostiniAppModelBuilder::CrostiniFolderObserver
    : public AppListModelUpdaterObserver {
 public:
  explicit CrostiniFolderObserver(CrostiniAppModelBuilder* parent)
      : parent_(parent) {}

  ~CrostiniFolderObserver() override = default;

  void OnAppListItemAdded(ChromeAppListItem* item) override {
    if (item->id() != crostini::kCrostiniFolderId)
      return;
    // Persistence is not recorded by the sync, so we always set it.
    item->SetIsPersistent(true);

    // Either the name and position will be in the sync, or we set them
    // manually.
    if (parent_->GetSyncItem(crostini::kCrostiniFolderId))
      return;
    item->SetName(
        l10n_util::GetStringUTF8(IDS_APP_LIST_CROSTINI_DEFAULT_FOLDER_NAME));
    item->SetDefaultPositionIfApplicable(parent_->model_updater());
  }

 private:
  CrostiniAppModelBuilder* parent_;
};

CrostiniAppModelBuilder::CrostiniAppModelBuilder(
    AppListControllerDelegate* controller)
    : AppListModelBuilder(controller, CrostiniAppItem::kItemType) {}

CrostiniAppModelBuilder::~CrostiniAppModelBuilder() {
  if (crostini_folder_observer_)
    model_updater()->RemoveObserver(crostini_folder_observer_.get());

  crostini::CrostiniRegistryServiceFactory::GetForProfile(profile())
      ->RemoveObserver(this);
}

void CrostiniAppModelBuilder::BuildModel() {
  crostini::CrostiniRegistryService* registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(profile());
  for (const auto& pair : registry_service->GetRegisteredApps()) {
    const std::string& app_id = pair.first;
    const auto& registration = pair.second;
    InsertCrostiniAppItem(app_id, registration);
  }

  registry_service->AddObserver(this);

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile()->GetPrefs());
  pref_change_registrar_->Add(
      crostini::prefs::kCrostiniEnabled,
      base::BindRepeating(&CrostiniAppModelBuilder::OnCrostiniEnabledChanged,
                          base::Unretained(this)));

  // We register an observer against the model_updater in order to track
  // creation and deletion of the crostini folder.
  if (model_updater()) {
    crostini_folder_observer_ = std::make_unique<CrostiniFolderObserver>(this);
    model_updater()->AddObserver(crostini_folder_observer_.get());
  }
}

void CrostiniAppModelBuilder::InsertCrostiniAppItem(
    const std::string& app_id,
    const crostini::CrostiniRegistryService::Registration& registration) {
  if (app_id == crostini::kCrostiniTerminalId &&
      !crostini::CrostiniFeatures::Get()->IsEnabled(profile())) {
    // If Crostini isn't enabled, don't show the Terminal item until it
    // becomes enabled.
    return;
  }
  if (registration.NoDisplay())
    return;

  InsertApp(std::make_unique<CrostiniAppItem>(profile(), model_updater(),
                                              GetSyncItem(app_id), app_id,
                                              registration.Name()));
}

void CrostiniAppModelBuilder::OnRegistryUpdated(
    crostini::CrostiniRegistryService* registry_service,
    const std::vector<std::string>& updated_apps,
    const std::vector<std::string>& removed_apps,
    const std::vector<std::string>& inserted_apps) {
  const bool unsynced_change = false;
  for (const std::string& app_id : removed_apps)
    RemoveApp(app_id, unsynced_change);
  for (const std::string& app_id : updated_apps) {
    auto registration = registry_service->GetRegistration(app_id);
    if (!registration.has_value() || registration->NoDisplay()) {
      if (!registration.has_value()) {
        LOG(ERROR)
            << "App was listed as updated by the registry, but was removed";
      }
      RemoveApp(app_id, unsynced_change);
      continue;
    }

    CrostiniAppItem* app_item =
        static_cast<CrostiniAppItem*>(GetAppItem(app_id));
    if (!app_item) {
      InsertCrostiniAppItem(app_id, *registration);
      continue;
    }
    app_item->SetName(registration->Name());
  }
  for (const std::string& app_id : inserted_apps) {
    // If the app has been installed before and has not been cleaned up
    // correctly, it needs to be removed.
    RemoveApp(app_id, unsynced_change);
    const auto& registration = registry_service->GetRegistration(app_id);
    if (registration.has_value()) {
      InsertCrostiniAppItem(app_id, *registration);
    } else {
      LOG(ERROR) << "CrostiniRegistryService reported app inserted that has no "
                    "registration";
    }
  }
}

void CrostiniAppModelBuilder::OnAppIconUpdated(const std::string& app_id,
                                               ui::ScaleFactor scale_factor) {
  CrostiniAppItem* app_item = static_cast<CrostiniAppItem*>(GetAppItem(app_id));
  if (!app_item) {
    VLOG(2) << "Could not update the icon of Crostini app (" << app_id
            << ") because it was not found";
    return;
  }

  // Initiate async icon reloading.
  app_item->crostini_app_icon()->LoadForScaleFactor(scale_factor);
}

void CrostiniAppModelBuilder::OnCrostiniEnabledChanged() {
  const bool unsynced_change = false;
  if (crostini::CrostiniFeatures::Get()->IsEnabled(profile())) {
    // If Terminal has been installed before and has not been cleaned up
    // correctly, it needs to be removed.
    RemoveApp(crostini::kCrostiniTerminalId, unsynced_change);
    crostini::CrostiniRegistryService* registry_service =
        crostini::CrostiniRegistryServiceFactory::GetForProfile(profile());
    const auto& registration =
        registry_service->GetRegistration(crostini::kCrostiniTerminalId);
    if (registration.has_value()) {
      InsertCrostiniAppItem(crostini::kCrostiniTerminalId, *registration);
    } else {
      LOG(ERROR) << "kCrostiniTerminalId has no registration";
    }
  } else {
    RemoveApp(crostini::kCrostiniTerminalId, unsynced_change);
  }
}
