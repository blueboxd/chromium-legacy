// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_TEST_FAKE_VR_DISPLAY_IMPL_CLIENT_H_
#define DEVICE_VR_TEST_FAKE_VR_DISPLAY_IMPL_CLIENT_H_

#include "device/vr/public/mojom/vr_service.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace device {
class FakeVRServiceClient;

class FakeVRDisplayImplClient : public mojom::VRDisplayClient,
                                public mojom::XRSessionClient {
 public:
  FakeVRDisplayImplClient(mojom::VRDisplayClientRequest request);
  ~FakeVRDisplayImplClient() override;

  void SetServiceClient(FakeVRServiceClient* service_client);
  // mojom::XRSessionClient overrides
  void OnChanged(mojom::VRDisplayInfoPtr display) override;
  void OnExitPresent() override {}
  void OnVisibilityStateChanged(
      mojom::XRVisibilityState visibility_state) override {}
  // mojom::VRDisplayClient overrides
  void OnActivate(mojom::VRDisplayEventReason reason,
                  OnActivateCallback callback) override {}
  void OnDeactivate(mojom::VRDisplayEventReason reason) override {}

 private:
  FakeVRServiceClient* service_client_;
  mojom::VRDisplayInfoPtr last_display_;
  mojo::Binding<mojom::VRDisplayClient> m_binding_;

  DISALLOW_COPY_AND_ASSIGN(FakeVRDisplayImplClient);
};

}  // namespace device

#endif  // DEVICE_VR_TEST_FAKE_VR_DISPLAY_IMPL_CLIENT_H_
