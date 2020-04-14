// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_SERVER_PRINTERS_PROVIDER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_SERVER_PRINTERS_PROVIDER_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace base {
template <typename T>
class NoDestructor;
}

namespace chromeos {

class ServerPrintersProvider;

class ServerPrintersProviderFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ServerPrintersProviderFactory* GetInstance();
  static ServerPrintersProvider* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend class base::NoDestructor<ServerPrintersProviderFactory>;

  ServerPrintersProviderFactory();
  ~ServerPrintersProviderFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(ServerPrintersProviderFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_SERVER_PRINTERS_PROVIDER_FACTORY_H_
