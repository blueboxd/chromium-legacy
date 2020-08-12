// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/service_worker_client_info.h"

namespace content {

ServiceWorkerClientInfo::ServiceWorkerClientInfo(int frame_tree_node_id)
    : type_(blink::mojom::ServiceWorkerClientType::kWindow),
      frame_tree_node_id_(frame_tree_node_id) {}

ServiceWorkerClientInfo::ServiceWorkerClientInfo(
    const blink::DedicatedWorkerToken& dedicated_worker_token)
    : type_(blink::mojom::ServiceWorkerClientType::kDedicatedWorker),
      worker_token_(dedicated_worker_token) {}

ServiceWorkerClientInfo::ServiceWorkerClientInfo(
    const blink::SharedWorkerToken& shared_worker_token)
    : type_(blink::mojom::ServiceWorkerClientType::kSharedWorker),
      worker_token_(shared_worker_token) {}

ServiceWorkerClientInfo::ServiceWorkerClientInfo(
    const DedicatedOrSharedWorkerToken& worker_token)
    : worker_token_(worker_token) {
  if (worker_token.Is<blink::DedicatedWorkerToken>()) {
    type_ = blink::mojom::ServiceWorkerClientType::kDedicatedWorker;
  } else {
    DCHECK(worker_token.Is<blink::SharedWorkerToken>());
    type_ = blink::mojom::ServiceWorkerClientType::kSharedWorker;
  }
}

ServiceWorkerClientInfo::ServiceWorkerClientInfo(
    const ServiceWorkerClientInfo& other) = default;

ServiceWorkerClientInfo& ServiceWorkerClientInfo::operator=(
    const ServiceWorkerClientInfo& other) = default;

ServiceWorkerClientInfo::~ServiceWorkerClientInfo() = default;

int ServiceWorkerClientInfo::GetFrameTreeNodeId() const {
  DCHECK_EQ(type_, blink::mojom::ServiceWorkerClientType::kWindow);
  return frame_tree_node_id_;
}

blink::DedicatedWorkerToken ServiceWorkerClientInfo::GetDedicatedWorkerToken()
    const {
  DCHECK_EQ(type_, blink::mojom::ServiceWorkerClientType::kDedicatedWorker);
  return worker_token_->GetAs<blink::DedicatedWorkerToken>();
}

blink::SharedWorkerToken ServiceWorkerClientInfo::GetSharedWorkerToken() const {
  DCHECK_EQ(type_, blink::mojom::ServiceWorkerClientType::kSharedWorker);
  return worker_token_->GetAs<blink::SharedWorkerToken>();
}

}  // namespace content
