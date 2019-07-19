// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_INTERNALS_SERVICE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_INTERNALS_SERVICE_H_

#include "base/macros.h"
#include "components/autofill/core/browser/logging/log_buffer.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/keyed_service/core/keyed_service.h"

namespace autofill {

// TODO(crbug.com/928595) This is a temporary home for these operations.
// They are not really associated with the AutofillInternalsService class.
LogBuffer& operator<<(LogBuffer& buf, LoggingScope scope);

LogBuffer& operator<<(LogBuffer& buf, LogMessage message);

// TODO(crbug.com/928595) This class is just a KeyedService version of
// autofill::LogRouter. Either make the LogRouter a KeyedService or at least
// unify this class with PasswordManagerInternalsService.

// Collects the logs for the autofill internals page and distributes them to all
// open tabs with the internals page.
class AutofillInternalsService : public KeyedService,
                                 public autofill::LogRouter {
 public:
  // There are only two ways in which the service depends on the BrowserContext:
  // 1) There is one service per each non-incognito BrowserContext.
  // 2) No service will be created for an incognito BrowserContext.
  // Both properties are guaranteed by the BrowserContextKeyedFactory framework,
  // so the service itself does not need the context on creation.
  AutofillInternalsService();
  ~AutofillInternalsService() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillInternalsService);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_INTERNALS_SERVICE_H_
