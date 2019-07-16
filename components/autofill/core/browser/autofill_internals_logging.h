// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_INTERNALS_LOGGING_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_INTERNALS_LOGGING_H_

#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/values.h"
#include "components/autofill/core/browser/logging/log_buffer.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"

//
// Autofill Internals Logging
// --------------------------
// This framework serves the purpose of exposing log messages of Chrome Autofill
// (and in the future also Chrome's Password Manager) via
// chrome://autofill-internals (and in the future also
// chrome://password-manager-internals).
//
// The framework uses a so called LoggingScope as a mechanism to indicate during
// which phase of processing a website a message was generated. Logging scopes
// comprise for example the phase of parsing forms, or filling forms. Each
// logging scope is displayed in a visually distinct color.
//
// The framework also uses predefined LogMessages. Each message consists of a
// fixed label and a string. The labels allow for specialized styling (e.g.
// highlighting specific messages because they indicate failures).
//
// The desired pattern to generate log messages is to pass a scope, a log
// message and then parameters.
//
// LOG_AF_INTERNALS << LoggingScope::kSomeScope << LogMessage::kSomeLogMessage
//     << Br{} << more << Br{} << parameters;
//
// Note that each call of LOG_AF_INTERNALS spawns a new log entry.
//
// See components/autofill/core/common/autofill_internals/logging_scope.h for
// the definition of scopes.
// See components/autofill/core/common/autofill_internals/log_message.h for
// the definition of messages.

namespace autofill {

LogBuffer& operator<<(LogBuffer& buf, LoggingScope scope);

LogBuffer& operator<<(LogBuffer& buf, LogMessage message);

// A container for a LogBuffer that submits the buffer to the
// corresponding internals page on destruction.
class AutofillInternalsMessage {
 public:
  // Whether the message will be sent to chrome://autofill-internals or
  // chrome://password-manager-internals.
  enum class Destination {
    kAutofillInternals,
    kPasswordManagerInternals,
  };

  explicit AutofillInternalsMessage(Destination destination);
  ~AutofillInternalsMessage();

  LogBuffer& buffer() { return buffer_; }

 private:
  Destination destination_;
  LogBuffer buffer_;
  DISALLOW_COPY_AND_ASSIGN(AutofillInternalsMessage);
};

// Class that forwards log messages to the WebUI for display.
class AutofillInternalsLogging {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void Log(const base::Value& message) = 0;
    virtual void LogRaw(const base::Value& message) = 0;
  };

  AutofillInternalsLogging();
  virtual ~AutofillInternalsLogging();

  void AddObserver(AutofillInternalsLogging::Observer* observer);
  void RemoveObserver(const Observer* observer);
  bool HasObservers() const;

  // Main API function that is called when something is logged.
  static void Log(const std::string& message);

  static AutofillInternalsLogging* GetInstance();

 private:
  static void LogRaw(const base::Value& message);

  // Grant access to LogRaw().
  friend class AutofillInternalsMessage;

  base::ObserverList<AutofillInternalsLogging::Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(AutofillInternalsLogging);
};

bool IsLogAutofillInternalsActive();

#define LOG_AF_INTERNALS                                         \
  AutofillInternalsMessage(                                      \
      AutofillInternalsMessage::Destination::kAutofillInternals) \
      .buffer()

#define LOG_PWMGR_INTERNALS                                             \
  AutofillInternalsMessage(                                             \
      AutofillInternalsMessage::Destination::kPasswordManagerInternals) \
      .buffer()

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_INTERNALS_LOGGING_H_
