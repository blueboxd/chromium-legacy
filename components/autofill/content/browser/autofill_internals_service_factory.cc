// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/autofill_internals_service_factory.h"

#include "components/autofill/core/browser/autofill_internals_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace autofill {

// static
AutofillInternalsService* AutofillInternalsServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AutofillInternalsService*>(
      GetInstance()->GetServiceForBrowserContext(context, /* create = */ true));
}

// static
AutofillInternalsServiceFactory*
AutofillInternalsServiceFactory::GetInstance() {
  return base::Singleton<AutofillInternalsServiceFactory>::get();
}

AutofillInternalsServiceFactory::AutofillInternalsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "AutofillInternalsService",
          BrowserContextDependencyManager::GetInstance()) {}

AutofillInternalsServiceFactory::~AutofillInternalsServiceFactory() {}

KeyedService* AutofillInternalsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* /* context */) const {
  return new AutofillInternalsService();
}

}  // namespace autofill
