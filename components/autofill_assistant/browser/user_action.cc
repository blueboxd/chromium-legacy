// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/user_action.h"

namespace autofill_assistant {

UserAction::UserAction(UserAction&& other) = default;
UserAction::UserAction() = default;
UserAction::~UserAction() = default;
UserAction& UserAction::operator=(UserAction&& other) = default;

// Initializes user action from proto.
UserAction::UserAction(const ChipProto& chip_proto,
                       const DirectActionProto& direct_action)
    : chip(chip_proto) {
  for (const std::string& name : direct_action.names()) {
    direct_action_names.emplace_back(name);
  }
}

bool UserAction::has_triggers() const {
  return !chip.empty() || !direct_action_names.empty();
}

}  // namespace autofill_assistant
