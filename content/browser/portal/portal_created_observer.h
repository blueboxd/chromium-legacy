// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PORTAL_PORTAL_CREATED_OBSERVER_H_
#define CONTENT_BROWSER_PORTAL_PORTAL_CREATED_OBSERVER_H_

#include "content/common/frame.mojom-test-utils.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/mojom/portal/portal.mojom.h"

namespace base {
class RunLoop;
}  // namespace base

namespace content {

class Portal;
class RenderFrameHostImpl;

// The PortalCreatedObserver observes portal creations on
// |render_frame_host_impl|. This observer can be used to monitor for multiple
// Portal creations on the same RenderFrameHost, by repeatedly calling
// WaitUntilPortalCreated().
class PortalCreatedObserver : public mojom::FrameHostInterceptorForTesting {
 public:
  explicit PortalCreatedObserver(RenderFrameHostImpl* render_frame_host_impl);
  ~PortalCreatedObserver() override;

  // mojom::FrameHostInterceptorForTesting
  mojom::FrameHost* GetForwardingInterface() override;
  void CreatePortal(
      mojo::PendingAssociatedReceiver<blink::mojom::Portal> portal,
      mojo::PendingAssociatedRemote<blink::mojom::PortalClient> client,
      CreatePortalCallback callback) override;
  void AdoptPortal(const base::UnguessableToken& portal_token,
                   AdoptPortalCallback callback) override;

  Portal* WaitUntilPortalCreated();

 private:
  RenderFrameHostImpl* render_frame_host_impl_;
  mojom::FrameHost* old_impl_;
  base::RunLoop* run_loop_ = nullptr;
  Portal* portal_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PORTAL_PORTAL_CREATED_OBSERVER_H_
