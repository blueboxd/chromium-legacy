// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_client_info.h"

namespace content {

ServiceWorkerClientInfo::ServiceWorkerClientInfo(
    int process_id,
    int route_id,
    const base::RepeatingCallback<WebContents*(void)>& web_contents_getter,
    blink::mojom::ServiceWorkerClientType type)
    : process_id(process_id),
      route_id(route_id),
      web_contents_getter(web_contents_getter),
      type(type) {
  DCHECK_NE(type, blink::mojom::ServiceWorkerClientType::kAll);
}

ServiceWorkerClientInfo::ServiceWorkerClientInfo(
    const ServiceWorkerClientInfo& other) = default;

ServiceWorkerClientInfo::~ServiceWorkerClientInfo() = default;

}  // namespace content
