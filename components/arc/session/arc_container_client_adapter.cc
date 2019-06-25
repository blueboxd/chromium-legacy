// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/arc_container_client_adapter.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/login_manager/arc.pb.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/dbus/upstart/upstart_client.h"

namespace arc {

class ArcContainerClientAdapter
    : public ArcClientAdapter,
      public chromeos::SessionManagerClient::Observer,
      public chromeos::UpstartClient::Observer {
 public:
  ArcContainerClientAdapter() {
    if (chromeos::SessionManagerClient::Get())
      chromeos::SessionManagerClient::Get()->AddObserver(this);
    if (chromeos::UpstartClient::Get())
      chromeos::UpstartClient::Get()->AddObserver(this);
  }

  ~ArcContainerClientAdapter() override {
    if (chromeos::SessionManagerClient::Get())
      chromeos::SessionManagerClient::Get()->RemoveObserver(this);
    if (chromeos::UpstartClient::Get())
      chromeos::UpstartClient::Get()->RemoveObserver(this);
  }

  // ArcClientAdapter overrides:
  void StartMiniArc(const StartArcMiniContainerRequest& request,
                    chromeos::VoidDBusMethodCallback callback) override {
    chromeos::SessionManagerClient::Get()->StartArcMiniContainer(
        request, std::move(callback));
  }

  void UpgradeArc(const UpgradeArcContainerRequest& request,
                  chromeos::VoidDBusMethodCallback callback) override {
    chromeos::SessionManagerClient::Get()->UpgradeArcContainer(
        request, std::move(callback));
  }

  void StopArcInstance() override {
    // Since we have the ArcInstanceStopped() callback, we don't need to do
    // anything when StopArcInstance completes.
    chromeos::SessionManagerClient::Get()->StopArcInstance(
        chromeos::EmptyVoidDBusMethodCallback());
  }

  void SetUserIdHashForProfile(const std::string& hash) override {}

  // chromeos::SessionManagerClient::Observer overrides:
  void ArcInstanceStopped() override {
    for (auto& observer : observer_list_)
      observer.ArcInstanceStopped();
  }

  // chromeos::UpstartClient::Observer overrides:
  void ArcStopped() override {
    for (auto& observer : observer_list_)
      observer.ArcInstanceStopped();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcContainerClientAdapter);
};

std::unique_ptr<ArcClientAdapter> CreateArcContainerClientAdapter() {
  return std::make_unique<ArcContainerClientAdapter>();
}

}  // namespace arc
