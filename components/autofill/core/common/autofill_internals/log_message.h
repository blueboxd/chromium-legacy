// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_INTERNALS_LOG_MESSAGE_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_INTERNALS_LOG_MESSAGE_H_

namespace autofill {

/////////////// Log Messages /////////////

// Generator for log message. If you need to find the call site for a log
// message, take the first parameter (e.g. ParsedForms) and search for
// that name prefixed with a k (e.g. kParsedForms) in code search.
#define AUTOFILL_LOG_MESSAGE_TEMPLATES(T) T(ParsedForms, "Parsed forms:")

// Log messages for chrome://autofill-internals.
#define AUTOFILL_TEMPLATE(NAME, MESSAGE) k##NAME,
enum class LogMessage {
  AUTOFILL_LOG_MESSAGE_TEMPLATES(AUTOFILL_TEMPLATE) kLastMessage
};
#undef AUTOFILL_TEMPLATE

// Returns the enum value of |message| as a string (without the leading k).
const char* LogMessageToString(LogMessage message);
// Returns the actual string to be presented to the user for |message|.
const char* LogMessageValue(LogMessage message);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_INTERNALS_LOG_MESSAGE_H_
