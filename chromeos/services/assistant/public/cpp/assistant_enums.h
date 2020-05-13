// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_ENUMS_H_
#define CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_ENUMS_H_

namespace chromeos {
namespace assistant {

// The initial state is NOT_READY, after Assistant service started it becomes
// READY. When Assistant UI shows up the state becomes VISIBLE.
enum AssistantStatus {
  // The Assistant service is not ready yet.
  NOT_READY = 0,
  // The Assistant service is ready.
  READY,
  // The Assistant service is ready.
  // TODO(b/142428671): This is set when the Assistant backend signals it is
  // ready to handle queries, where |READY| is signaled when it is started.
  // If this signal turns out to be reliable it should replace |READY| and be
  // renamed to |READY|.
  NEW_READY,
};

enum AssistantAllowedState {
  // Assistant feature is allowed.
  ALLOWED = 0,
  // Disallowed because search and assistant is disabled by policy.
  DISALLOWED_BY_POLICY,
  // Disallowed because user's locale is not compatible.
  DISALLOWED_BY_LOCALE,
  // Disallowed because current user is not primary user.
  DISALLOWED_BY_NONPRIMARY_USER,
  // Disallowed because current user is supervised user.
  DISALLOWED_BY_SUPERVISED_USER,
  // Disallowed because incognito mode.
  DISALLOWED_BY_INCOGNITO,
  // Disallowed because the device is in demo mode.
  DISALLOWED_BY_DEMO_MODE,
  // Disallowed because the device is in public session.
  DISALLOWED_BY_PUBLIC_SESSION,
  // Disallowed because the user's account type is currently not supported.
  DISALLOWED_BY_ACCOUNT_TYPE,
  // Disallowed because the device is in Kiosk mode.
  DISALLOWED_BY_KIOSK_MODE,

  MAX_VALUE = DISALLOWED_BY_KIOSK_MODE,
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_ENUMS_H_
