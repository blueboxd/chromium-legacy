// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/service_worker_client_info.h"

namespace content {

ServiceWorkerClientInfo::ServiceWorkerClientInfo(int frame_tree_node_id)
    : type_(blink::mojom::ServiceWorkerClientType::kWindow),
      frame_tree_node_id_(frame_tree_node_id) {}
ServiceWorkerClientInfo::ServiceWorkerClientInfo(
    DedicatedWorkerId dedicated_worker_id)
    : type_(blink::mojom::ServiceWorkerClientType::kDedicatedWorker),
      dedicated_worker_id_(dedicated_worker_id) {}
ServiceWorkerClientInfo::ServiceWorkerClientInfo(
    SharedWorkerId shared_worker_id)
    : type_(blink::mojom::ServiceWorkerClientType::kSharedWorker),
      shared_worker_id_(shared_worker_id) {}

ServiceWorkerClientInfo::ServiceWorkerClientInfo(
    const ServiceWorkerClientInfo& other) = default;

ServiceWorkerClientInfo& ServiceWorkerClientInfo::operator=(
    const ServiceWorkerClientInfo& other) = default;

ServiceWorkerClientInfo::~ServiceWorkerClientInfo() = default;

int ServiceWorkerClientInfo::GetFrameTreeNodeId() const {
  DCHECK_EQ(type_, blink::mojom::ServiceWorkerClientType::kWindow);
  return frame_tree_node_id_;
}

DedicatedWorkerId ServiceWorkerClientInfo::GetDedicatedWorkerId() const {
  DCHECK_EQ(type_, blink::mojom::ServiceWorkerClientType::kDedicatedWorker);
  return dedicated_worker_id_;
}

SharedWorkerId ServiceWorkerClientInfo::GetSharedWorkerId() const {
  DCHECK_EQ(type_, blink::mojom::ServiceWorkerClientType::kSharedWorker);
  return shared_worker_id_;
}

}  // namespace content
