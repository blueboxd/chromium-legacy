// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_SERVICE_WORKER_CLIENT_H_
#define COMPONENTS_PERFORMANCE_MANAGER_SERVICE_WORKER_CLIENT_H_

#include "content/public/browser/dedicated_worker_id.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/shared_worker_id.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom.h"

// Represents a client of a service worker node.
//
// This class is essentially a tagged union where only the field corresponding
// to the |type()| can be accessed.
class ServiceWorkerClient {
 public:
  explicit ServiceWorkerClient(
      content::GlobalFrameRoutingId render_frame_host_id);
  explicit ServiceWorkerClient(content::DedicatedWorkerId dedicated_worker_id);
  explicit ServiceWorkerClient(content::SharedWorkerId shared_worker_id);

  ServiceWorkerClient(const ServiceWorkerClient& other);
  ServiceWorkerClient& operator=(const ServiceWorkerClient& other);

  ~ServiceWorkerClient();

  blink::mojom::ServiceWorkerClientType type() const { return type_; }

  content::GlobalFrameRoutingId GetRenderFrameHostId() const;
  content::DedicatedWorkerId GetDedicatedWorkerId() const;
  content::SharedWorkerId GetSharedWorkerId() const;

 private:
  // The client type.
  blink::mojom::ServiceWorkerClientType type_;

  // The frame tree node ID, if this is a window client.
  content::GlobalFrameRoutingId render_frame_host_id_;

  union {
    // The ID of the client, if this is a dedicated worker client.
    content::DedicatedWorkerId dedicated_worker_id_;

    // The ID of the client, if this is a shared worker client.
    content::SharedWorkerId shared_worker_id_;
  };
};

#endif  // COMPONENTS_PERFORMANCE_MANAGER_SERVICE_WORKER_CLIENT_H_
