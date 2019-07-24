// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSION_MANAGER_CORE_SESSION_MANAGER_OBSERVER_H_
#define COMPONENTS_SESSION_MANAGER_CORE_SESSION_MANAGER_OBSERVER_H_

#include "base/macros.h"
#include "base/observer_list_types.h"
#include "components/session_manager/session_manager_types.h"

namespace session_manager {

// An observer interface for SessionManager.
// TODO(xiyuan): Use this to replace UserManager::UserSessionStateObserver,
//     http://crbug.com/657149.
class SessionManagerObserver : public base::CheckedObserver {
 public:
  // Inovked when session manager is destroyed.
  virtual void OnSessionManagerDestroyed() {}

  // Invoked when session state is changed.
  virtual void OnSessionStateChanged() {}

  // Invoked when a user profile is loaded.
  virtual void OnUserProfileLoaded(const AccountId& account_id) {}

  // Invoked when the primary user session is started.
  virtual void OnPrimaryUserSessionStarted() {}
};

}  // namespace session_manager

#endif  // COMPONENTS_SESSION_MANAGER_CORE_SESSION_MANAGER_OBSERVER_H_
