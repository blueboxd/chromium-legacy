// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CLIENT_INFO_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CLIENT_INFO_H_

#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom.h"

namespace content {

// Holds information about a single service worker client:
// https://w3c.github.io/ServiceWorker/#client
struct CONTENT_EXPORT ServiceWorkerClientInfo {
  ServiceWorkerClientInfo(blink::mojom::ServiceWorkerClientType type,
                          int frame_tree_node_id);
  ServiceWorkerClientInfo(const ServiceWorkerClientInfo& other);
  ~ServiceWorkerClientInfo();

  // The client type.
  blink::mojom::ServiceWorkerClientType type;

  int frame_tree_node_id;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CLIENT_INFO_H_
