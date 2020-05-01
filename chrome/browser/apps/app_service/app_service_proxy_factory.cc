// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry_factory.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "extensions/browser/app_window/app_window_registry.h"
#endif  // OS_CHROMEOS

namespace apps {

// static
AppServiceProxy* AppServiceProxyFactory::GetForProfile(Profile* profile) {
  // TODO: decide the right behaviour in incognito (non-guest) profiles:
  //   - return nullptr (means we need to null check the service at call sites
  //     OR ensure it's never accessed from an incognito profile),
  //   - return the service attached to the Profile that the incognito profile
  //     is branched from (i.e. "inherit" the parent service),
  //   - return a temporary service just for the incognito session (probably
  //     the least sensible option).
  return static_cast<AppServiceProxy*>(
      AppServiceProxyFactory::GetInstance()->GetServiceForBrowserContext(
          profile, true /* create */));
}

// static
AppServiceProxyFactory* AppServiceProxyFactory::GetInstance() {
  return base::Singleton<AppServiceProxyFactory>::get();
}

AppServiceProxyFactory::AppServiceProxyFactory()
    : BrowserContextKeyedServiceFactory(
          "AppServiceProxy",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionPrefsFactory::GetInstance());
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(web_app::WebAppProviderFactory::GetInstance());
#if defined(OS_CHROMEOS)
  DependsOn(guest_os::GuestOsRegistryServiceFactory::GetInstance());
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
  DependsOn(extensions::AppWindowRegistry::Factory::GetInstance());
#endif  // OS_CHROMEOS
}

AppServiceProxyFactory::~AppServiceProxyFactory() = default;

KeyedService* AppServiceProxyFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AppServiceProxy(Profile::FromBrowserContext(context));
}

content::BrowserContext* AppServiceProxyFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  Profile* const profile = Profile::FromBrowserContext(context);
  if (!profile || profile->IsSystemProfile()) {
    return nullptr;
  }

#if defined(OS_CHROMEOS)
  if (chromeos::ProfileHelper::IsSigninProfile(profile)) {
    return nullptr;
  }

  // We must have a proxy in guest mode to ensure default extension-based apps
  // are served. Otherwise, don't create the app service for incognito profiles.
  if (profile->IsGuestSession()) {
    return chrome::GetBrowserContextOwnInstanceInIncognito(context);
  }
#endif  // OS_CHROMEOS

  return BrowserContextKeyedServiceFactory::GetBrowserContextToUse(context);
}

bool AppServiceProxyFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace apps
