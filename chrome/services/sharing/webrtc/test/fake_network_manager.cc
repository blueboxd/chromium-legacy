// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/webrtc/test/fake_network_manager.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"

namespace sharing {

FakeNetworkManager::FakeNetworkManager(const rtc::IPAddress& address) {
  network_ =
      std::make_unique<rtc::Network>("fake", "Fake Network", address, 32);
  network_->AddIP(address);
}

FakeNetworkManager::~FakeNetworkManager() = default;

void FakeNetworkManager::StartUpdating() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FakeNetworkManager::SendNetworksChangedSignal,
                                weak_factory_.GetWeakPtr()));
}

void FakeNetworkManager::StopUpdating() {}

void FakeNetworkManager::GetNetworks(NetworkList* networks) const {
  networks->clear();
  networks->push_back(network_.get());
}

void FakeNetworkManager::SendNetworksChangedSignal() {
  SignalNetworksChanged();
}

}  // namespace sharing
