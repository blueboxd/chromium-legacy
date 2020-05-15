// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_client_info.h"

namespace content {

ServiceWorkerClientInfo::ServiceWorkerClientInfo(
    blink::mojom::ServiceWorkerClientType type,
    int frame_tree_node_id)
    : type(type), frame_tree_node_id(frame_tree_node_id) {
  DCHECK_NE(type, blink::mojom::ServiceWorkerClientType::kAll);
}

ServiceWorkerClientInfo::ServiceWorkerClientInfo(
    const ServiceWorkerClientInfo& other) = default;

ServiceWorkerClientInfo::~ServiceWorkerClientInfo() = default;

}  // namespace content
