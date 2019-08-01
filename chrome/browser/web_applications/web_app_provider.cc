// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/install_bounce_metric.h"
#include "chrome/browser/web_applications/components/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/components/web_app_audio_focus_id_map.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_install_finalizer.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_registrar.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_tab_helper.h"
#include "chrome/browser/web_applications/external_web_app_manager.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "chrome/browser/web_applications/pending_app_manager_impl.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_database.h"
#include "chrome/browser/web_applications/web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_manager.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/web_contents.h"

#define DCHECK_IS_CONNECTED()                                          \
  DCHECK(connected_) << "Attempted to access Web App subsystem while " \
                        "WebAppProvider is not connected."

namespace web_app {

// static
WebAppProvider* WebAppProvider::Get(Profile* profile) {
  return WebAppProviderFactory::GetForProfile(profile);
}

// static
WebAppProvider* WebAppProvider::GetForWebContents(
    content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DCHECK(profile);
  return WebAppProvider::Get(profile);
}

WebAppProvider::WebAppProvider(Profile* profile) : profile_(profile) {
  DCHECK(AreWebAppsEnabled(profile_));
  // WebApp System must have only one instance in original profile.
  // Exclude secondary off-the-record profiles.
  DCHECK(!profile_->IsOffTheRecord());

  CreateCommonSubsystems(profile_);

  if (base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions))
    CreateWebAppsSubsystems(profile_);
  else
    CreateBookmarkAppsSubsystems(profile_);
}

WebAppProvider::~WebAppProvider() = default;

void WebAppProvider::Start() {
  CHECK(!started_);

  ConnectSubsystems();
  started_ = true;

  StartImpl();
}

AppRegistrar& WebAppProvider::registrar() {
  DCHECK_IS_CONNECTED();
  return *registrar_;
}

InstallManager& WebAppProvider::install_manager() {
  DCHECK_IS_CONNECTED();
  return *install_manager_;
}

PendingAppManager& WebAppProvider::pending_app_manager() {
  DCHECK_IS_CONNECTED();
  return *pending_app_manager_;
}

WebAppPolicyManager* WebAppProvider::policy_manager() {
  DCHECK_IS_CONNECTED();
  return web_app_policy_manager_.get();
}

WebAppUiManager& WebAppProvider::ui_manager() {
  DCHECK(ui_manager_);
  return *ui_manager_;
}

SystemWebAppManager& WebAppProvider::system_web_app_manager() {
  DCHECK_IS_CONNECTED();
  return *system_web_app_manager_;
}

void WebAppProvider::Shutdown() {
  // Destroy subsystems.
  // The order of destruction is the reverse order of creation:
  // TODO(calamity): Make subsystem destruction happen in destructor.
  ui_manager_.reset();
  system_web_app_manager_.reset();
  web_app_policy_manager_.reset();
  external_web_app_manager_.reset();
  pending_app_manager_.reset();

  install_manager_.reset();
  install_finalizer_.reset();
  icon_manager_.reset();
  registrar_.reset();
  database_.reset();
  database_factory_.reset();
  audio_focus_id_map_.reset();
}

void WebAppProvider::StartImpl() {
  StartRegistry();
}

void WebAppProvider::CreateCommonSubsystems(Profile* profile) {
  audio_focus_id_map_ = std::make_unique<WebAppAudioFocusIdMap>();
  install_manager_ = std::make_unique<WebAppInstallManager>(profile);
  pending_app_manager_ = std::make_unique<PendingAppManagerImpl>(profile);
  ui_manager_ = WebAppUiManager::Create(profile);
}

void WebAppProvider::CreateWebAppsSubsystems(Profile* profile) {
  database_factory_ = std::make_unique<WebAppDatabaseFactory>(profile);
  database_ = std::make_unique<WebAppDatabase>(database_factory_.get());
  auto web_app_registrar =
      std::make_unique<WebAppRegistrar>(profile, database_.get());
  icon_manager_ = std::make_unique<WebAppIconManager>(
      profile, std::make_unique<FileUtilsWrapper>());

  // TODO(crbug.com/973324): Once the WebAppInstallFinalizer can take an
  // AppRegistrar instead of needing a WebAppRegistrar, move this wiring into
  // ConnectSubsystems().
  install_finalizer_ = std::make_unique<WebAppInstallFinalizer>(
      web_app_registrar.get(), icon_manager_.get());
  sync_manager_ = std::make_unique<WebAppSyncManager>();

  system_web_app_manager_ = std::make_unique<SystemWebAppManager>(profile);

  registrar_ = std::move(web_app_registrar);
}

void WebAppProvider::CreateBookmarkAppsSubsystems(Profile* profile) {
  install_finalizer_ =
      std::make_unique<extensions::BookmarkAppInstallFinalizer>(profile);

  external_web_app_manager_ = std::make_unique<ExternalWebAppManager>(profile);

  web_app_policy_manager_ = std::make_unique<WebAppPolicyManager>(profile);

  system_web_app_manager_ = std::make_unique<SystemWebAppManager>(profile);

  registrar_ = std::make_unique<extensions::BookmarkAppRegistrar>(profile);
}

void WebAppProvider::ConnectSubsystems() {
  DCHECK(!started_);

  install_manager_->SetSubsystems(registrar_.get(), install_finalizer_.get());
  install_finalizer_->SetSubsystems(ui_manager_.get());

  // TODO(crbug.com/877898): Port all other managers to support BMO.
  if (!base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions)) {
    pending_app_manager_->SetSubsystems(registrar_.get(), ui_manager_.get(),
                                        install_finalizer_.get());
    external_web_app_manager_->SetSubsystems(pending_app_manager_.get());
    web_app_policy_manager_->SetSubsystems(pending_app_manager_.get());
  }
  system_web_app_manager_->SetSubsystems(pending_app_manager_.get(),
                                         registrar_.get(), ui_manager_.get());

  connected_ = true;
}

void WebAppProvider::StartRegistry() {
  registrar_->Init(base::BindOnce(&WebAppProvider::OnRegistryReady,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void WebAppProvider::OnRegistryReady() {
  DCHECK(!on_registry_ready_.is_signaled());

  if (!base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions)) {
    external_web_app_manager_->Start();
    web_app_policy_manager_->Start();
    system_web_app_manager_->Start();
  }

  on_registry_ready_.Signal();
}

// static
void WebAppProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  ExternallyInstalledWebAppPrefs::RegisterProfilePrefs(registry);
  WebAppPolicyManager::RegisterProfilePrefs(registry);
  SystemWebAppManager::RegisterProfilePrefs(registry);
  RegisterInstallBounceMetricProfilePrefs(registry);
}

// static
WebAppTabHelperBase* WebAppProvider::CreateTabHelper(
    content::WebContents* web_contents) {
  WebAppProvider* provider = WebAppProvider::GetForWebContents(web_contents);
  if (!provider)
    return nullptr;

  WebAppTabHelperBase* tab_helper =
      WebAppTabHelperBase::FromWebContents(web_contents);
  // Do nothing if already exists.
  if (tab_helper)
    return tab_helper;

  if (base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions)) {
    tab_helper = WebAppTabHelper::CreateForWebContents(web_contents);
  } else {
    tab_helper =
        extensions::BookmarkAppTabHelper::CreateForWebContents(web_contents);
  }

  tab_helper->Init(provider->audio_focus_id_map_.get());
  return tab_helper;
}

}  // namespace web_app
