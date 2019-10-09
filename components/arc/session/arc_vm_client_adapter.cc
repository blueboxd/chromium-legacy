// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/arc_vm_client_adapter.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromeos/dbus/concierge_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/login_manager/arc.pb.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "components/arc/arc_util.h"

namespace arc {
namespace {

chromeos::ConciergeClient* GetConciergeClient() {
  return chromeos::DBusThreadManager::Get()->GetConciergeClient();
}

}  // namespace

class ArcVmClientAdapter : public ArcClientAdapter,
                           public chromeos::ConciergeClient::VmObserver {
 public:
  ArcVmClientAdapter() { GetConciergeClient()->AddVmObserver(this); }
  ~ArcVmClientAdapter() override {
    GetConciergeClient()->RemoveVmObserver(this);
  }

  // chromeos::ConciergeClient::VmObserver overrides:
  void OnVmStarted(
      const vm_tools::concierge::VmStartedSignal& signal) override {
    if (signal.name() == kArcVmName)
      VLOG(1) << "OnVmStarted: ARCVM cid=" << signal.vm_info().cid();
  }

  void OnVmStopped(
      const vm_tools::concierge::VmStoppedSignal& signal) override {
    if (signal.name() != kArcVmName)
      return;
    VLOG(1) << "OnVmStopped: ARCVM";
    OnArcInstanceStopped();
  }

  // ArcClientAdapter overrides:
  void StartMiniArc(const StartArcMiniContainerRequest& request,
                    chromeos::VoidDBusMethodCallback callback) override {
    // TODO(yusukes): Support mini ARC.
    VLOG(2) << "Mini ARCVM instance is not supported.";
    // Save the lcd density and auto update mode for the later call to
    // UpgradeArc.
    lcd_density_ = request.lcd_density();
    cpus_ = base::SysInfo::NumberOfProcessors() - request.num_cores_disabled();
    DCHECK_LT(0u, cpus_);
    if (request.play_store_auto_update() ==
        login_manager::
            StartArcMiniContainerRequest_PlayStoreAutoUpdate_AUTO_UPDATE_ON) {
      play_store_auto_update_ = true;
    } else if (
        request.play_store_auto_update() ==
        login_manager::
            StartArcMiniContainerRequest_PlayStoreAutoUpdate_AUTO_UPDATE_OFF) {
      play_store_auto_update_ = false;
    }
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
  }

  void UpgradeArc(const UpgradeArcContainerRequest& request,
                  chromeos::VoidDBusMethodCallback callback) override {
    VLOG(1) << "Starting ARCVM";
    std::vector<std::string> env{
        {"USER_ID_HASH=" + user_id_hash_},
        {base::StringPrintf("ARC_LCD_DENSITY=%d", lcd_density_)},
        {base::StringPrintf("CPUS=%u", cpus_)},
    };
    if (play_store_auto_update_) {
      env.push_back(base::StringPrintf("PLAY_STORE_AUTO_UPDATE=%d",
                                       *play_store_auto_update_));
    }
    chromeos::UpstartClient::Get()->StartJob("arcvm", env, std::move(callback));
  }

  void StopArcInstance() override {
    VLOG(1) << "Stopping arcvm";
    std::vector<std::string> env{{"USER_ID_HASH=" + user_id_hash_}};
    chromeos::UpstartClient::Get()->StopJob(
        "arcvm", env,
        base::BindOnce(&ArcVmClientAdapter::OnArcVmJobStopped,
                       weak_factory_.GetWeakPtr()));
  }

  void SetUserIdHashForProfile(const std::string& hash) override {
    DCHECK(!hash.empty());
    user_id_hash_ = hash;
  }

 private:
  void OnArcInstanceStopped() {
    VLOG(1) << "arcvm stopped.";
    for (auto& observer : observer_list_)
      observer.ArcInstanceStopped();
  }

  // TODO(yusukes): Remove this method when we remove arcvm.conf.
  void OnArcVmJobStopped(bool result) {
    if (!result)
      LOG(ERROR) << "Failed to stop arcvm.";
  }

  // A hash of the primary profile user ID.
  std::string user_id_hash_;

  int32_t lcd_density_;
  uint32_t cpus_;

  base::Optional<bool> play_store_auto_update_;

  // For callbacks.
  base::WeakPtrFactory<ArcVmClientAdapter> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcVmClientAdapter);
};

std::unique_ptr<ArcClientAdapter> CreateArcVmClientAdapter() {
  return std::make_unique<ArcVmClientAdapter>();
}

}  // namespace arc
