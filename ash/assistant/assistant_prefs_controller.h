// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_PREFS_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_PREFS_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/session/session_observer.h"
#include "base/macros.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"

class PrefChangeRegistrar;

namespace ash {

// A checked observer which receives Assistant prefs change.
class ASH_EXPORT AssistantPrefsObserver : public base::CheckedObserver {
 public:
  AssistantPrefsObserver() = default;
  ~AssistantPrefsObserver() override = default;

  virtual void OnAssistantConsentStatusUpdated(int consent_status) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantPrefsObserver);
};

class ASH_EXPORT AssistantPrefsController : public SessionObserver {
 public:
  AssistantPrefsController();
  ~AssistantPrefsController() override;

  void AddObserver(AssistantPrefsObserver* observer);
  void RemoveObserver(AssistantPrefsObserver* observer);
  void InitObserver(AssistantPrefsObserver* observer);

  PrefService* prefs();

 private:
  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // Called when the consent status is obtained from the pref service.
  void NotifyConsentStatus();

  // Observes user profile prefs for the Assistant.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  ScopedSessionObserver session_observer_;

  base::ObserverList<AssistantPrefsObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AssistantPrefsController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_PREFS_CONTROLLER_H_
