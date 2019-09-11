// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_POSSIBLE_USERNAME_DATA_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_POSSIBLE_USERNAME_DATA_H_

#include <string>

#include "base/strings/string16.h"
#include "base/time/time.h"

namespace password_manager {

// The maximum time between the user typed in a text field and subsequent
// submission of the password form, such that the typed value is considered to
// be possible to be username.
constexpr base::TimeDelta kMaxDelayBetweenTypingUsernameAndSubmission =
    base::TimeDelta::FromSeconds(60);

// Contains information that the user typed in a text field. It might be the
// username during username first flow.
struct PossibleUsernameData {
  PossibleUsernameData(std::string signon_realm,
                       int32_t renderer_id,
                       base::string16 value,
                       base::Time last_change);
  ~PossibleUsernameData();

  std::string signon_realm;
  int32_t renderer_id;
  base::string16 value;
  base::Time last_change;
};

// Checks that |possible_username| might represent an username:
// 1.|possible_username.signon_realm| == |submitted_signon_realm|
// 2.|possible_username.value| does not have whitespaces and non-ASCII symbols.
// 3.|possible_username.value.last_change| was not more than 60 seconds before
// now.
bool IsPossibleUsernameValid(const PossibleUsernameData& possible_username,
                             const std::string& submitted_signon_realm,
                             const base::Time now);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_POSSIBLE_USERNAME_DATA_H_
