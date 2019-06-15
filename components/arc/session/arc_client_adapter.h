// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_SESSION_ARC_CLIENT_ADAPTER_H_
#define COMPONENTS_ARC_SESSION_ARC_CLIENT_ADAPTER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"

namespace arc {

using StartArcMiniContainerRequest =
    login_manager::StartArcMiniContainerRequest;
using UpgradeArcContainerRequest = login_manager::UpgradeArcContainerRequest;

// An adapter to talk to a Chrome OS daemon to manage lifetime of ARC instance.
class ArcClientAdapter {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void ArcInstanceStopped() = 0;
  };

  // Creates a default instance of ArcClientAdapter.
  static std::unique_ptr<ArcClientAdapter> Create();
  virtual ~ArcClientAdapter();

  // StartMiniArc starts ARC with only a handful of ARC processes for Chrome OS
  // login screen.
  virtual void StartMiniArc(const StartArcMiniContainerRequest& request,
                            chromeos::VoidDBusMethodCallback callback) = 0;

  // UpgradeArc upgrades a mini ARC instance to a full ARC instance.
  virtual void UpgradeArc(const UpgradeArcContainerRequest& request,
                          chromeos::VoidDBusMethodCallback callback) = 0;

  // Asynchronously stops the ARC instance.
  virtual void StopArcInstance() = 0;

  // Sets a hash string of the profile user ID.
  virtual void SetUserIdHashForProfile(const std::string& hash) = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  ArcClientAdapter();

  base::ObserverList<Observer>::Unchecked observer_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcClientAdapter);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_SESSION_ARC_CLIENT_ADAPTER_H_
