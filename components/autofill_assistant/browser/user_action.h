// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_ACTION_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "components/autofill_assistant/browser/chip.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

// An action that the user can perform, through the UI or, on Android Q, through
// a direct action.
struct UserAction {
  UserAction(UserAction&&);
  UserAction();
  ~UserAction();
  UserAction& operator=(UserAction&&);

  // Initializes user action from proto.
  UserAction(const ChipProto& chip, const DirectActionProto& direct_action);

  // Returns true if the action has no trigger, that is, there is no chip and no
  // direct action.
  bool has_triggers() const;

  // Specifies how the user can perform the action through the UI. Might be
  // empty.
  Chip chip;

  // Names of the direct action under which this action is available. Optional.
  std::vector<std::string> direct_action_names;

  // Whether the action is enabled. The chip for a disabled action might still
  // be shown.
  bool enabled = true;

  // Callback triggered to trigger the action.
  base::OnceClosure callback;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_ACTION_H_
