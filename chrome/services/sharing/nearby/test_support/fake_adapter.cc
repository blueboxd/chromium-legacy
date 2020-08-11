// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/test_support/fake_adapter.h"

#include <memory>

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace bluetooth {

namespace {

class FakeDiscoverySession : public mojom::DiscoverySession {
 public:
  explicit FakeDiscoverySession(base::OnceClosure on_destroy_callback)
      : on_destroy_callback_(std::move(on_destroy_callback)) {}
  ~FakeDiscoverySession() override { std::move(on_destroy_callback_).Run(); }

 private:
  // mojom::FakeDiscoverySession:
  void IsActive(IsActiveCallback callback) override {
    std::move(callback).Run(true);
  }
  void Stop(StopCallback callback) override { std::move(callback).Run(true); }

  base::OnceClosure on_destroy_callback_;
};

}  // namespace

FakeAdapter::FakeAdapter() = default;

FakeAdapter::~FakeAdapter() = default;

void FakeAdapter::ConnectToDevice(const std::string& address,
                                  ConnectToDeviceCallback callback) {}

void FakeAdapter::GetDevices(GetDevicesCallback callback) {}

void FakeAdapter::GetInfo(GetInfoCallback callback) {
  mojom::AdapterInfoPtr adapter_info = mojom::AdapterInfo::New();
  adapter_info->name = name_;
  adapter_info->present = present_;
  adapter_info->powered = powered_;
  adapter_info->discoverable = discoverable_;
  adapter_info->discovering = discovering_;
  std::move(callback).Run(std::move(adapter_info));
}

void FakeAdapter::SetClient(::mojo::PendingRemote<mojom::AdapterClient> client,
                            SetClientCallback callback) {
  client_.Bind(std::move(client));
  std::move(callback).Run();
}

void FakeAdapter::StartDiscoverySession(
    StartDiscoverySessionCallback callback) {
  DCHECK(!discovery_session_);

  if (!should_discovery_succeed_) {
    std::move(callback).Run(std::move(mojo::NullRemote()));
    return;
  }

  auto discovery_session =
      std::make_unique<FakeDiscoverySession>(base::BindOnce(
          &FakeAdapter::OnDiscoverySessionDestroyed, base::Unretained(this)));
  discovery_session_ = discovery_session.get();

  mojo::PendingRemote<mojom::DiscoverySession> pending_session;
  mojo::MakeSelfOwnedReceiver(std::move(discovery_session),
                              pending_session.InitWithNewPipeAndPassReceiver());

  std::move(callback).Run(std::move(pending_session));
}

void FakeAdapter::SetShouldDiscoverySucceed(bool should_discovery_succeed) {
  should_discovery_succeed_ = should_discovery_succeed;
}

void FakeAdapter::SetDiscoverySessionDestroyedCallback(
    base::OnceClosure callback) {
  on_discovery_session_destroyed_callback_ = std::move(callback);
}

bool FakeAdapter::IsDiscoverySessionActive() {
  return discovery_session_;
}

void FakeAdapter::NotifyDeviceAdded(mojom::DeviceInfoPtr device_info) {
  client_->DeviceAdded(std::move(device_info));
}

void FakeAdapter::NotifyDeviceChanged(mojom::DeviceInfoPtr device_info) {
  client_->DeviceChanged(std::move(device_info));
}

void FakeAdapter::NotifyDeviceRemoved(mojom::DeviceInfoPtr device_info) {
  client_->DeviceRemoved(std::move(device_info));
}

void FakeAdapter::OnDiscoverySessionDestroyed() {
  DCHECK(discovery_session_);
  discovery_session_ = nullptr;
  if (on_discovery_session_destroyed_callback_)
    std::move(on_discovery_session_destroyed_callback_).Run();
}

void FakeAdapter::ConnectToServiceInsecurely(
    const std::string& address,
    const device::BluetoothUUID& service_uuid,
    ConnectToServiceInsecurelyCallback callback) {}

}  // namespace bluetooth
