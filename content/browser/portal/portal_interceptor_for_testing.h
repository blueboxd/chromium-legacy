// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PORTAL_PORTAL_INTERCEPTOR_FOR_TESTING_H_
#define CONTENT_BROWSER_PORTAL_PORTAL_INTERCEPTOR_FOR_TESTING_H_

#include <memory>

#include "base/callback.h"
#include "content/browser/portal/portal.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/mojom/portal/portal.mojom-test-utils.h"
#include "third_party/blink/public/mojom/portal/portal.mojom.h"

namespace base {
class RunLoop;
}  // namespace base

namespace content {

class RenderFrameHostImpl;

// The PortalInterceptorForTesting can be used in tests to inspect Portal IPCs.
class PortalInterceptorForTesting final
    : public blink::mojom::PortalInterceptorForTesting {
 public:
  static PortalInterceptorForTesting* Create(
      RenderFrameHostImpl* render_frame_host_impl,
      mojo::PendingAssociatedReceiver<blink::mojom::Portal> receiver,
      mojo::AssociatedRemote<blink::mojom::PortalClient> client);
  static PortalInterceptorForTesting* Create(
      RenderFrameHostImpl* render_frame_host_impl,
      content::Portal* portal);
  static PortalInterceptorForTesting* From(content::Portal* portal);

  ~PortalInterceptorForTesting() override;

  // blink::mojom::PortalInterceptorForTesting
  blink::mojom::Portal* GetForwardingInterface() override;
  void Activate(blink::TransferableMessage data,
                ActivateCallback callback) override;
  void Navigate(const GURL& url,
                blink::mojom::ReferrerPtr referrer,
                blink::mojom::Portal::NavigateCallback callback) override;

  // If set, will be used to replace the implementation of Navigate.
  using NavigateCallback =
      base::RepeatingCallback<void(const GURL&,
                                   blink::mojom::ReferrerPtr,
                                   blink::mojom::Portal::NavigateCallback)>;
  void SetNavigateCallback(NavigateCallback callback) {
    navigate_callback_ = std::move(callback);
  }

  void WaitForActivate();

  // Test getters.
  content::Portal* GetPortal() { return portal_.get(); }
  WebContentsImpl* GetPortalContents() { return portal_->GetPortalContents(); }

 private:
  explicit PortalInterceptorForTesting(
      RenderFrameHostImpl* render_frame_host_impl);
  PortalInterceptorForTesting(RenderFrameHostImpl* render_frame_host_impl,
                              std::unique_ptr<content::Portal> portal);

  std::unique_ptr<content::Portal> portal_;
  NavigateCallback navigate_callback_;
  bool portal_activated_ = false;
  base::RunLoop* run_loop_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PORTAL_PORTAL_INTERCEPTOR_FOR_TESTING_H_
