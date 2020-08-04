// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CONTROLLER_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/observer_list.h"
#include "components/account_id/account_id.h"

namespace ash {

class HoldingSpaceControllerObserver;
class HoldingSpaceModel;

// Keeps track of all registered holding space models per user account and makes
// sure the current active model belongs to the current active user.
// There is expected to exist at most one instance of this class at a time. In
// production the instance is owned by ash::Shell. The instance can be retrieved
// using HoldingSpaceController::Get().
class ASH_PUBLIC_EXPORT HoldingSpaceController : public SessionObserver {
 public:
  HoldingSpaceController();
  HoldingSpaceController(const HoldingSpaceController& other) = delete;
  HoldingSpaceController& operator=(const HoldingSpaceController& other) =
      delete;
  ~HoldingSpaceController() override;

  // Returns the global HoldingSpaceController instance. It's set in the
  // HoldingSpaceController constructor, and reset in the destructor. The
  // instance is owned by ash shell.
  static HoldingSpaceController* Get();

  void AddObserver(HoldingSpaceControllerObserver* observer);
  void RemoveObserver(HoldingSpaceControllerObserver* observer);

  // Adds a model to it's corresponding user account id in a map.
  void RegisterModelForUser(const AccountId& account_id,
                            HoldingSpaceModel* model);

  // Sets the active model - the caller is expected to maintain the ownership.
  void SetModel(HoldingSpaceModel* model);

  HoldingSpaceModel* model() { return model_; }

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

 private:
  // The currently active holding space model, set by SetModel(). The client
  // that sets the model is expected to maintain the model ownership.
  HoldingSpaceModel* model_ = nullptr;

  // The currently active user account id.
  AccountId active_user_account_id_;

  std::map<const AccountId, HoldingSpaceModel*> models_by_account_id_;

  base::ObserverList<HoldingSpaceControllerObserver> observers_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CONTROLLER_H_
