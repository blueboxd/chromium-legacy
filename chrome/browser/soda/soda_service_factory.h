// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SODA_SODA_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SODA_SODA_SERVICE_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace base {
template <class T>
class NoDestructor;
}  // namespace base

namespace soda {
class SodaService;
}  // namespace soda

// Factory to get or create an instance of SodaServiceFactory from
// a Profile.
class SodaServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static soda::SodaService* GetForProfile(Profile* profile);

 private:
  friend class base::NoDestructor<SodaServiceFactory>;
  static SodaServiceFactory* GetInstance();

  SodaServiceFactory();
  ~SodaServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_SODA_SODA_SERVICE_FACTORY_H_
