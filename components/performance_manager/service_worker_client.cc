// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/service_worker_client.h"

ServiceWorkerClient::ServiceWorkerClient(
    content::GlobalFrameRoutingId render_frame_host_id)
    : type_(blink::mojom::ServiceWorkerClientType::kWindow),
      render_frame_host_id_(render_frame_host_id) {}
ServiceWorkerClient::ServiceWorkerClient(
    content::DedicatedWorkerId dedicated_worker_id)
    : type_(blink::mojom::ServiceWorkerClientType::kDedicatedWorker),
      dedicated_worker_id_(dedicated_worker_id) {}
ServiceWorkerClient::ServiceWorkerClient(
    content::SharedWorkerId shared_worker_id)
    : type_(blink::mojom::ServiceWorkerClientType::kSharedWorker),
      shared_worker_id_(shared_worker_id) {}

ServiceWorkerClient::ServiceWorkerClient(const ServiceWorkerClient& other) =
    default;
ServiceWorkerClient& ServiceWorkerClient::operator=(
    const ServiceWorkerClient& other) = default;

ServiceWorkerClient::~ServiceWorkerClient() = default;

content::GlobalFrameRoutingId ServiceWorkerClient::GetRenderFrameHostId()
    const {
  DCHECK_EQ(type_, blink::mojom::ServiceWorkerClientType::kWindow);
  return render_frame_host_id_;
}

content::DedicatedWorkerId ServiceWorkerClient::GetDedicatedWorkerId() const {
  DCHECK_EQ(type_, blink::mojom::ServiceWorkerClientType::kDedicatedWorker);
  return dedicated_worker_id_;
}

content::SharedWorkerId ServiceWorkerClient::GetSharedWorkerId() const {
  DCHECK_EQ(type_, blink::mojom::ServiceWorkerClientType::kSharedWorker);
  return shared_worker_id_;
}
