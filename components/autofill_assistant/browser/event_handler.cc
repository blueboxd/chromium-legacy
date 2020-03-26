// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/event_handler.h"

namespace autofill_assistant {

EventHandler::EventHandler() = default;
EventHandler::~EventHandler() = default;

void EventHandler::DispatchEvent(const EventKey& key) {
  DVLOG(3) << __func__ << " " << key;
  for (auto& observer : observers_) {
    observer.OnEvent(key);
  }
}

void EventHandler::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void EventHandler::RemoveObserver(const Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::ostream& operator<<(std::ostream& out,
                         const EventProto::KindCase& event_case) {
#ifdef NDEBUG
  out << static_cast<int>(event_case);
  return out;
#else
  switch (event_case) {
    case EventProto::kOnValueChanged:
      out << "kOnValueChanged";
      break;
    case EventProto::kOnViewClicked:
      out << "kOnViewClicked";
      break;
    case EventProto::kOnUserActionCalled:
      out << "kOnUserActionCalled";
      break;
    case EventProto::kOnTextLinkClicked:
      out << "kOnTextLinkClicked";
      break;
    case EventProto::KIND_NOT_SET:
      break;
  }
  return out;
#endif
}

std::ostream& operator<<(std::ostream& out, const EventHandler::EventKey& key) {
  out << "{" << key.first << ", " << key.second << "}";
  return out;
}

}  // namespace autofill_assistant
