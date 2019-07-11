// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_internals/log_message.h"

#include "base/logging.h"

namespace autofill {

const char* LogMessageToString(LogMessage message) {
  // Define template to generate case-statements:
#define AUTOFILL_TEMPLATE(NAME, STRING) \
  case LogMessage::k##NAME:             \
    return #NAME;

  // The actual implementation of this function.
  switch (message) {
    AUTOFILL_LOG_MESSAGE_TEMPLATES(AUTOFILL_TEMPLATE)
    case LogMessage::kLastMessage:
      return "";
      // No default here to cover all cases.
  }

    // Clean up.
#undef AUTOFILL_TEMPLATE

  NOTREACHED();
  return "";
}

const char* LogMessageValue(LogMessage message) {
  // Define template to generate case-statements:
#define AUTOFILL_TEMPLATE(NAME, STRING) \
  case LogMessage::k##NAME:             \
    return STRING;

  // The actual implementation of this function.
  switch (message) {
    AUTOFILL_LOG_MESSAGE_TEMPLATES(AUTOFILL_TEMPLATE)
    case LogMessage::kLastMessage:
      return "";
      // No default here to cover all cases.
  }

    // Clean up.
#undef AUTOFILL_TEMPLATE

  NOTREACHED();
  return "";
}

}  // namespace autofill
