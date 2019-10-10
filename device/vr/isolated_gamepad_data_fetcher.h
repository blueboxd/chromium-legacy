// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ISOLATED_GAMEPAD_DATA_FETCHER_H_
#define DEVICE_VR_ISOLATED_GAMEPAD_DATA_FETCHER_H_

#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace device {

class IsolatedGamepadDataFetcher : public GamepadDataFetcher {
 public:
  class DEVICE_VR_EXPORT Factory : public GamepadDataFetcherFactory {
   public:
    Factory(device::mojom::XRDeviceId display_id,
            mojo::PendingRemote<device::mojom::IsolatedXRGamepadProviderFactory>
                factory);
    ~Factory() override;
    std::unique_ptr<GamepadDataFetcher> CreateDataFetcher() override;
    GamepadSource source() override;

    static void AddGamepad(
        device::mojom::XRDeviceId device_id,
        mojo::PendingRemote<device::mojom::IsolatedXRGamepadProviderFactory>
            gamepad_factory);
    static void RemoveGamepad(device::mojom::XRDeviceId device_id);

   private:
    device::mojom::XRDeviceId display_id_;
    mojo::Remote<device::mojom::IsolatedXRGamepadProviderFactory> factory_;
  };

  IsolatedGamepadDataFetcher(
      device::mojom::XRDeviceId display_id,
      mojo::PendingRemote<device::mojom::IsolatedXRGamepadProvider> provider);
  ~IsolatedGamepadDataFetcher() override;

  GamepadSource source() override;

  void GetGamepadData(bool devices_changed_hint) override;
  void PauseHint(bool paused) override;
  void OnAddedToProvider() override;

  void OnDataUpdated(device::mojom::XRGamepadDataPtr data);

 private:
  device::mojom::XRDeviceId display_id_;
  bool have_outstanding_request_ = false;
  std::set<unsigned int> active_gamepads_;
  device::mojom::XRGamepadDataPtr data_;
  mojo::Remote<device::mojom::IsolatedXRGamepadProvider>
      provider_;  // Bound on the polling thread.
  mojo::PendingRemote<device::mojom::IsolatedXRGamepadProvider>
      pending_provider_;  // Received on the UI thread, bound when polled.

  DISALLOW_COPY_AND_ASSIGN(IsolatedGamepadDataFetcher);
};

}  // namespace device
#endif  // DEVICE_VR_ISOLATED_GAMEPAD_DATA_FETCHER_H_
