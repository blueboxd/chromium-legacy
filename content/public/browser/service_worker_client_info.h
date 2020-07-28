// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CLIENT_INFO_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CLIENT_INFO_H_

#include "content/common/content_export.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/tokens/worker_tokens.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom.h"

namespace content {

// Holds information about a single service worker client:
// https://w3c.github.io/ServiceWorker/#client
class CONTENT_EXPORT ServiceWorkerClientInfo {
 public:
  explicit ServiceWorkerClientInfo(int frame_tree_node_id);
  explicit ServiceWorkerClientInfo(
      const blink::DedicatedWorkerToken& dedicated_worker_token);
  explicit ServiceWorkerClientInfo(
      const blink::SharedWorkerToken& shared_worker_token);

  ServiceWorkerClientInfo(const ServiceWorkerClientInfo& other);
  ServiceWorkerClientInfo& operator=(const ServiceWorkerClientInfo& other);

  ~ServiceWorkerClientInfo();

  // Returns the type of this client.
  blink::mojom::ServiceWorkerClientType type() const { return type_; }

  int GetFrameTreeNodeId() const;
  const blink::DedicatedWorkerToken& GetDedicatedWorkerToken() const;
  const blink::SharedWorkerToken& GetSharedWorkerToken() const;

 private:
  // The client type.
  blink::mojom::ServiceWorkerClientType type_;

  // The frame tree node ID, if this is a window client.
  int frame_tree_node_id_ = content::RenderFrameHost::kNoFrameTreeNodeId;

  // The ID of the client, if this is a dedicated worker client.
  blink::DedicatedWorkerToken dedicated_worker_token_;

  // The ID of the client, if this is a shared worker client.
  blink::SharedWorkerToken shared_worker_token_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CLIENT_INFO_H_
