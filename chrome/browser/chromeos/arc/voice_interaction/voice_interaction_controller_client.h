// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_VOICE_INTERACTION_VOICE_INTERACTION_CONTROLLER_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_ARC_VOICE_INTERACTION_VOICE_INTERACTION_CONTROLLER_CLIENT_H_

#include <memory>

#include "ash/public/mojom/assistant_state_controller.mojom.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/user_manager/user_manager.h"

namespace arc {

// TODO(b/138679823): Remove the class and use AssistantState instead.
// The client of VoiceInteractionController. It monitors various user session
// states and notifies Ash side.  It can also be used to notify some specific
// state changes that does not have an observer interface.
class VoiceInteractionControllerClient
    : public user_manager::UserManager::UserSessionStateObserver,
      public ArcSessionManager::Observer {
 public:
  class Observer {
   public:
    // Called when voice interaction session state changes.
    virtual void OnStateChanged(ash::mojom::AssistantState state) = 0;
  };

  VoiceInteractionControllerClient();
  ~VoiceInteractionControllerClient() override;
  static VoiceInteractionControllerClient* Get();

  // Notify the controller about state changes.
  void NotifyStatusChanged(ash::mojom::AssistantState state);

  void NotifyLockedFullScreenStateChanged(bool enabled);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  const ash::mojom::AssistantState& assistant_state() const {
    return assistant_state_;
  }

 private:
  friend class VoiceInteractionControllerClientTest;

  // Notify the controller about state changes.
  void NotifyFeatureAllowed();
  void NotifyLocaleChanged();

  // user_manager::UserManager::UserSessionStateObserver overrides:
  void ActiveUserChanged(user_manager::User* active_user) override;

  // Override ArcSessionManager::Observer
  void OnArcPlayStoreEnabledChanged(bool enabled) override;

  void SetProfileByUser(const user_manager::User* user);
  void SetProfile(Profile* profile);

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  Profile* profile_ = nullptr;

  ash::mojom::AssistantState assistant_state_ =
      ash::mojom::AssistantState::READY;

  base::ObserverList<Observer>::Unchecked observers_;

  base::WeakPtrFactory<VoiceInteractionControllerClient> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(VoiceInteractionControllerClient);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_VOICE_INTERACTION_VOICE_INTERACTION_CONTROLLER_CLIENT_H_
