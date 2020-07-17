// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_CHANGE_PASSWORD_URL_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_CHANGE_PASSWORD_URL_SERVICE_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace password_manager {
class ChangePasswordUrlService;
}

namespace content {
class BrowserContext;
}

// Creates instances of ChangePasswordUrl per BrowserContext.
class ChangePasswordUrlServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  ChangePasswordUrlServiceFactory();
  ~ChangePasswordUrlServiceFactory() override;

  static ChangePasswordUrlServiceFactory* GetInstance();
  static password_manager::ChangePasswordUrlService* GetForBrowserContext(
      content::BrowserContext* browser_context);

 private:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_CHANGE_PASSWORD_URL_SERVICE_FACTORY_H_
