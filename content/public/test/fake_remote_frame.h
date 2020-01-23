// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_FAKE_REMOTE_FRAME_H_
#define CONTENT_PUBLIC_TEST_FAKE_REMOTE_FRAME_H_

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "ui/events/types/scroll_types.h"

namespace base {
class UnguessableToken;
}

namespace url {
class Origin;
}  // namespace url

namespace content {

// This class implements a RemoteFrame that can be attached to the
// AssociatedInterfaceProvider so that it will be called when the browser
// normally sends a request to the renderer process. But for a unittest
// setup it can be intercepted by this class.
class FakeRemoteFrame : public blink::mojom::RemoteFrame {
 public:
  FakeRemoteFrame();
  ~FakeRemoteFrame() override;

  void Init(blink::AssociatedInterfaceProvider* provider);

  // blink::mojom::RemoteFrame overrides:
  void WillEnterFullscreen() override;
  void AddReplicatedContentSecurityPolicies(
      std::vector<network::mojom::ContentSecurityPolicyHeaderPtr> headers)
      override;
  void ResetReplicatedContentSecurityPolicy() override;
  void EnforceInsecureNavigationsSet(const std::vector<uint32_t>& set) override;
  void SetReplicatedOrigin(
      const url::Origin& origin,
      bool is_potentially_trustworthy_unique_origin) override;
  void DispatchLoadEventForFrameOwner() override;
  void Collapse(bool collapsed) final;
  void Focus() override;
  void SetHadStickyUserActivationBeforeNavigation(bool value) override;
  void SetNeedsOcclusionTracking(bool needs_tracking) override;
  void BubbleLogicalScroll(
      blink::mojom::ScrollDirection direction,
      ui::input_types::ScrollGranularity granularity) override;
  void UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType) override;
  void SetEmbeddingToken(
      const base::UnguessableToken& embedding_token) override;
  void SetPageFocus(bool is_focused) override;
  void RenderFallbackContent() override;
  void AddResourceTimingFromChild(
      blink::mojom::ResourceTimingInfoPtr timing) override;

 private:
  void BindFrameHostReceiver(mojo::ScopedInterfaceEndpointHandle handle);

  mojo::AssociatedReceiver<blink::mojom::RemoteFrame> receiver_{this};
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_FAKE_REMOTE_FRAME_H_
