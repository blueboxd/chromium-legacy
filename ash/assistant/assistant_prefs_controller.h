// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_PREFS_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_PREFS_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/assistant/assistant_state_base.h"
#include "ash/session/session_observer.h"
#include "base/macros.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"

class PrefChangeRegistrar;

namespace ash {

// Provide access of Assistant related preferences to clients in ash.
class ASH_EXPORT AssistantPrefsController : public SessionObserver,
                                            public AssistantStateBase {
 public:
  AssistantPrefsController();
  ~AssistantPrefsController() override;

  // AssistantStateBase:
  void InitializeObserver(AssistantStateObserver* observer) override;

 private:
  // Update pref cache and notify observers when primary user prefs becomes
  // active.
  void UpdateState();

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // Called when the related preferences are obtained from the pref service.
  void UpdateConsentStatus();
  void UpdateHotwordAlwaysOn();
  void UpdateLaunchWithMicOpen();
  void UpdateNotificationEnabled();

  // TODO(b/138679823): Move related logics into AssistantStateBase.
  // Observes user profile prefs for the Assistant.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  ScopedSessionObserver session_observer_;

  DISALLOW_COPY_AND_ASSIGN(AssistantPrefsController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_PREFS_CONTROLLER_H_
