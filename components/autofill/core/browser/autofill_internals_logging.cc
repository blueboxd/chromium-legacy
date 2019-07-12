// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_internals_logging.h"

#include <string>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/escape.h"

namespace autofill {

namespace {

void AppendChildToLastNode(std::vector<base::Value>* buffer,
                           base::Value&& new_child) {
  if (buffer->empty()) {
    buffer->push_back(std::move(new_child));
    return;
  }

  base::Value& parent = buffer->back();

  if (auto* children = parent.FindListKey("children")) {
    children->GetList().push_back(std::move(new_child));
    return;
  }

  base::Value::ListStorage list;
  list.emplace_back(std::move(new_child));
  parent.SetKey("children", base::Value(std::move(list)));
}

}  // namespace

AutofillInternalsBuffer::AutofillInternalsBuffer() = default;
AutofillInternalsBuffer::AutofillInternalsBuffer(
    AutofillInternalsBuffer&& other) noexcept = default;
AutofillInternalsBuffer::~AutofillInternalsBuffer() = default;

base::Value AutofillInternalsBuffer::RetrieveResult() {
  if (buffer_.empty())
    return base::Value();

  // Close not-yet-closed tags.
  while (buffer_.size() > 1)
    *this << CTag{};

  return std::exchange(buffer_.back(), base::Value());
}

AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf, Tag&& tag) {
  if (!buf.active())
    return buf;

  base::Value::DictStorage storage;
  storage.try_emplace("type", std::make_unique<base::Value>("node"));
  storage.try_emplace("value",
                      std::make_unique<base::Value>(std::move(tag.name)));
  buf.buffer_.emplace_back(std::move(storage));
  return buf;
}

AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf, CTag&& tag) {
  if (!buf.active())
    return buf;
  // Don't close the very first opened tag. It stays and gets returned in the
  // end.
  if (buf.buffer_.size() <= 1)
    return buf;

  base::Value node_to_add = std::move(buf.buffer_.back());
  buf.buffer_.pop_back();

  AppendChildToLastNode(&buf.buffer_, std::move(node_to_add));
  return buf;
}

AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf,
                                    Attrib&& attrib) {
  if (!buf.active())
    return buf;

  base::Value& node = buf.buffer_.back();

  if (auto* attributes = node.FindDictKey("attributes")) {
    attributes->SetKey(std::move(attrib.name),
                       base::Value(std::move(attrib.value)));
  } else {
    base::Value::DictStorage dict;
    dict.try_emplace(std::move(attrib.name),
                     std::make_unique<base::Value>(std::move(attrib.value)));
    node.SetKey("attributes", base::Value(std::move(dict)));
  }

  return buf;
}

AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf, Br&& tag) {
  if (!buf.active())
    return buf;
  return buf << Tag{"br"} << CTag{};
}

AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf,
                                    base::StringPiece text) {
  if (!buf.active())
    return buf;

  base::Value::DictStorage storage;
  storage.try_emplace("type", std::make_unique<base::Value>("text"));
  storage.try_emplace("value",
                      std::make_unique<base::Value>(net::EscapeForHTML(text)));
  base::Value node_to_add(std::move(storage));
  AppendChildToLastNode(&buf.buffer_, std::move(node_to_add));
  return buf;
}

AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf,
                                    base::StringPiece16 text) {
  return buf << base::UTF16ToUTF8(text);
}

AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf,
                                    AutofillInternalsBuffer&& buffer) {
  if (!buf.active())
    return buf;

  base::Value node_to_add(buffer.RetrieveResult());
  if (node_to_add.is_none())
    return buf;
  AppendChildToLastNode(&buf.buffer_, std::move(node_to_add));
  return buf;
}

AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf,
                                    LoggingScope scope) {
  if (!buf.active())
    return buf;
  return buf << Tag{"div"} << Attrib{"scope", LoggingScopeToString(scope)}
             << Attrib{"class", "log-entry"};
}

AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf,
                                    LogMessage message) {
  if (!buf.active())
    return buf;
  return buf << Tag{"div"} << Attrib{"message", LogMessageToString(message)}
             << Attrib{"class", "log-message"} << LogMessageValue(message);
}

AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf,
                                    const GURL& url) {
  if (!buf.active())
    return buf;
  if (!url.is_valid())
    return buf << "Invalid URL";
  return buf << net::EscapeForHTML(url.GetOrigin().spec());
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
