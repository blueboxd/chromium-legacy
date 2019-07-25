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

namespace autofill {

// TODO(crbug.com/928595) This is a temporary home for these operations.
// Find a properly named file.
LogBuffer& operator<<(LogBuffer& buf, LoggingScope scope);

LogBuffer& operator<<(LogBuffer& buf, LogMessage message);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_INTERNALS_SERVICE_H_
