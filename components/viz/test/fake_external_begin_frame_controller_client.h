// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_FAKE_EXTERNAL_BEGIN_FRAME_CONTROLLER_CLIENT_H_
#define COMPONENTS_VIZ_TEST_FAKE_EXTERNAL_BEGIN_FRAME_CONTROLLER_CLIENT_H_

#include "mojo/public/cpp/bindings/binding.h"
#include "services/viz/privileged/mojom/compositing/external_begin_frame_controller.mojom.h"

namespace viz {

class FakeExternalBeginFrameControllerClient
    : public mojom::ExternalBeginFrameControllerClient {
 public:
  FakeExternalBeginFrameControllerClient();
  ~FakeExternalBeginFrameControllerClient() override;

  mojom::ExternalBeginFrameControllerClientPtr BindInterfacePtr();

 private:
  // ExternalBeginFrameControllerClient implementation.
  void OnNeedsBeginFrames(bool needs_begin_frames) override;
  void OnDisplayDidFinishFrame(const BeginFrameAck& ack) override;

  mojo::Binding<mojom::ExternalBeginFrameControllerClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(FakeExternalBeginFrameControllerClient);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_FAKE_EXTERNAL_BEGIN_FRAME_CONTROLLER_CLIENT_H_
