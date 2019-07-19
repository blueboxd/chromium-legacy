// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_AUTOFILL_INTERNALS_SERVICE_FACTORY_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_AUTOFILL_INTERNALS_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace autofill {

class AutofillInternalsService;

// BrowserContextKeyedServiceFactory for AutofillInternalsService. It
// does not override BrowserContextKeyedServiceFactory::GetBrowserContextToUse,
// which means that no service is returned in Incognito.
class AutofillInternalsServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static AutofillInternalsService* GetForBrowserContext(
      content::BrowserContext* context);

  static AutofillInternalsServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<AutofillInternalsServiceFactory>;

  AutofillInternalsServiceFactory();
  ~AutofillInternalsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(AutofillInternalsServiceFactory);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_AUTOFILL_INTERNALS_SERVICE_FACTORY_H_
