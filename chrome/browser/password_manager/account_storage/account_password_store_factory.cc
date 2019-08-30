// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/account_storage/account_password_store_factory.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_data_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_default.h"
#include "components/password_manager/core/browser/password_store_factory_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"

using password_manager::PasswordStore;

// static
scoped_refptr<PasswordStore> AccountPasswordStoreFactory::GetForProfile(
    Profile* profile,
    ServiceAccessType access_type) {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kEnablePasswordsAccountStorage)) {
    return nullptr;
  }
  // |profile| gets always redirected to a non-Incognito profile below, so
  // Incognito & IMPLICIT_ACCESS means that incognito browsing session would
  // result in traces in the normal profile without the user knowing it.
  if (access_type == ServiceAccessType::IMPLICIT_ACCESS &&
      profile->IsOffTheRecord()) {
    return nullptr;
  }
  return base::WrapRefCounted(static_cast<password_manager::PasswordStore*>(
      GetInstance()->GetServiceForBrowserContext(profile, true).get()));
}

// static
AccountPasswordStoreFactory* AccountPasswordStoreFactory::GetInstance() {
  return base::Singleton<AccountPasswordStoreFactory>::get();
}

AccountPasswordStoreFactory::AccountPasswordStoreFactory()
    : RefcountedBrowserContextKeyedServiceFactory(
          "AccountPasswordStore",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(WebDataServiceFactory::GetInstance());
}

AccountPasswordStoreFactory::~AccountPasswordStoreFactory() {}

scoped_refptr<RefcountedKeyedService>
AccountPasswordStoreFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(base::FeatureList::IsEnabled(
      password_manager::features::kEnablePasswordsAccountStorage));

  Profile* profile = Profile::FromBrowserContext(context);

  std::unique_ptr<password_manager::LoginDatabase> login_db(
      password_manager::CreateLoginDatabaseForAccountStorage(
          profile->GetPath()));

  scoped_refptr<PasswordStore> ps =
      new password_manager::PasswordStoreDefault(std::move(login_db));
  if (!ps->Init(/*flare=*/base::DoNothing(), profile->GetPrefs())) {
    // TODO(crbug.com/479725): Remove the LOG once this error is visible in the
    // UI.
    LOG(WARNING) << "Could not initialize password store.";
    return nullptr;
  }

  auto network_context_getter = base::BindRepeating(
      [](Profile* profile) -> network::mojom::NetworkContext* {
        if (!g_browser_process->profile_manager()->IsValidProfile(profile))
          return nullptr;
        return content::BrowserContext::GetDefaultStoragePartition(profile)
            ->GetNetworkContext();
      },
      profile);
  password_manager_util::RemoveUselessCredentials(ps, profile->GetPrefs(), 60,
                                                  network_context_getter);

  return ps;
}

content::BrowserContext* AccountPasswordStoreFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool AccountPasswordStoreFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
