// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_internals_logging.h"

#include "base/no_destructor.h"

namespace autofill {

LogBuffer& operator<<(LogBuffer& buf, LoggingScope scope) {
  if (!buf.active())
    return buf;
  return buf << Tag{"div"} << Attrib{"scope", LoggingScopeToString(scope)}
             << Attrib{"class", "log-entry"};
}

LogBuffer& operator<<(LogBuffer& buf, LogMessage message) {
  if (!buf.active())
    return buf;
  return buf << Tag{"div"} << Attrib{"message", LogMessageToString(message)}
             << Attrib{"class", "log-message"} << LogMessageValue(message);
}

// Implementation of AutofillInternalsMessage.

AutofillInternalsMessage::AutofillInternalsMessage(Destination destination)
    : destination_(destination) {
  if (destination_ == Destination::kAutofillInternals)
    buffer_.set_active(IsLogAutofillInternalsActive());
}

AutofillInternalsMessage::~AutofillInternalsMessage() {
  base::Value message = buffer_.RetrieveResult();
  if (message.is_none())
    return;

  switch (destination_) {
    case Destination::kAutofillInternals:
      AutofillInternalsLogging::LogRaw(std::move(message));
      break;
    case Destination::kPasswordManagerInternals:
      LOG(ERROR) << "Not implemented, logging to password manager internals";
      break;
  }
}

// Implementation of AutofillInternalsLogging.

AutofillInternalsLogging::AutofillInternalsLogging() = default;

AutofillInternalsLogging::~AutofillInternalsLogging() = default;

// static
AutofillInternalsLogging* AutofillInternalsLogging::GetInstance() {
  static base::NoDestructor<AutofillInternalsLogging> logger;
  return logger.get();
}

void AutofillInternalsLogging::AddObserver(
    AutofillInternalsLogging::Observer* observer) {
  observers_.AddObserver(observer);
}

void AutofillInternalsLogging::RemoveObserver(
    const AutofillInternalsLogging::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool AutofillInternalsLogging::HasObservers() const {
  return observers_.might_have_observers();
}

// static
void AutofillInternalsLogging::Log(const std::string& message) {
  auto& observers = AutofillInternalsLogging::GetInstance()->observers_;
  if (!observers.might_have_observers())
    return;
  base::Value message_value(message);
  for (Observer& obs : observers)
    obs.Log(message_value);
}

// static
void AutofillInternalsLogging::LogRaw(const base::Value& message) {
  auto& observers = AutofillInternalsLogging::GetInstance()->observers_;
  for (Observer& obs : observers)
    obs.LogRaw(message);
}

// Implementation of other methods.

bool IsLogAutofillInternalsActive() {
  return AutofillInternalsLogging::GetInstance()->HasObservers();
}

}  // namespace autofill
