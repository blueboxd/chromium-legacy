// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_INSTALLER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_INSTALLER_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

class Profile;

namespace borealis {

class BorealisInstaller;

class BorealisInstallerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static BorealisInstaller* GetForProfile(Profile* profile);
  static BorealisInstallerFactory* GetInstance();

 private:
  friend base::NoDestructor<BorealisInstallerFactory>;

  BorealisInstallerFactory();
  ~BorealisInstallerFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(BorealisInstallerFactory);
};

}  // namespace borealis

#endif  // CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_INSTALLER_FACTORY_H_
