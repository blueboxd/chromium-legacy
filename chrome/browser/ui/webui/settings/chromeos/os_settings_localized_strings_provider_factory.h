// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_LOCALIZED_STRINGS_PROVIDER_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_LOCALIZED_STRINGS_PROVIDER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace chromeos {
namespace settings {

class OsSettingsLocalizedStringsProvider;

class OsSettingsLocalizedStringsProviderFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static OsSettingsLocalizedStringsProvider* GetForProfile(Profile* profile);
  static OsSettingsLocalizedStringsProviderFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      OsSettingsLocalizedStringsProviderFactory>;

  OsSettingsLocalizedStringsProviderFactory();
  ~OsSettingsLocalizedStringsProviderFactory() override;

  OsSettingsLocalizedStringsProviderFactory(
      const OsSettingsLocalizedStringsProviderFactory&) = delete;
  OsSettingsLocalizedStringsProviderFactory& operator=(
      const OsSettingsLocalizedStringsProviderFactory&) = delete;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_LOCALIZED_STRINGS_PROVIDER_FACTORY_H_
