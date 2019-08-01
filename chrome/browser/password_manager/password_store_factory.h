// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_FACTORY_H_

#include <string>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "components/keyed_service/content/refcounted_browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/service_access_type.h"

class PrefService;
class Profile;

namespace password_manager {
class PasswordStore;
}

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS) && defined(OS_POSIX)
// Local profile ids are used to associate resources stored outside the profile
// directory, like saved passwords in GNOME Keyring / KWallet, with a profile.
// With high probability, they are unique on the local machine. They are almost
// certainly not unique globally, by design. Do not send them over the network.
typedef int LocalProfileId;
#endif

// Singleton that owns all PasswordStores and associates them with
// Profiles.
class PasswordStoreFactory
    : public RefcountedBrowserContextKeyedServiceFactory {
 public:
  static scoped_refptr<password_manager::PasswordStore> GetForProfile(
      Profile* profile,
      ServiceAccessType set);

  static PasswordStoreFactory* GetInstance();

  // Called by the PasswordDataTypeController whenever there is a possibility
  // that syncing passwords has just started or ended for |profile|.
  static void OnPasswordsSyncedStatePotentiallyChanged(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<PasswordStoreFactory>;

  PasswordStoreFactory();
  ~PasswordStoreFactory() override;

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS) && defined(OS_POSIX)
  LocalProfileId GetLocalProfileId(PrefService* prefs) const;
#endif

  // RefcountedBrowserContextKeyedServiceFactory:
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreFactory);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_FACTORY_H_
