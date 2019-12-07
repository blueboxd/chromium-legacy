// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace chromeos {
namespace multidevice_setup {

class MultiDeviceSetupService;

class MultiDeviceSetupServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static MultiDeviceSetupService* GetForProfile(Profile* profile);

  static MultiDeviceSetupServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<MultiDeviceSetupServiceFactory>;

  MultiDeviceSetupServiceFactory();
  MultiDeviceSetupServiceFactory(const MultiDeviceSetupServiceFactory&) =
      delete;
  MultiDeviceSetupServiceFactory& operator=(
      const MultiDeviceSetupServiceFactory&) = delete;
  ~MultiDeviceSetupServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace multidevice_setup
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_SERVICE_FACTORY_H_
