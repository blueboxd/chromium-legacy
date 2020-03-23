// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_LOADER_HELPERS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_LOADER_HELPERS_H_

#include <memory>
#include <string>

#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

namespace base {
class TimeDelta;
}

namespace blink {
namespace mojom {
enum class ServiceWorkerUpdateViaCache;
}
}  // namespace blink

namespace content {

namespace service_worker_loader_helpers {

// Check if |response_head| is a valid response for a service worker script
// (e.g. bad mime type). Status codes and error message will be set when the
// response is invalid.
bool CheckResponseHead(
    const network::mojom::URLResponseHead& response_head,
    blink::ServiceWorkerStatusCode* out_service_worker_status,
    network::URLLoaderCompletionStatus* out_completion_status,
    std::string* out_error_message);

// Creates net::HttpResponseInfo from |response_head|. Returns nullptr when the
// response is invalid.
// TODO(crbug.com/1060076): Remove this once HttpResponseInfo dependencies are
// gone.
std::unique_ptr<net::HttpResponseInfo> CreateHttpResponseInfoAndCheckHeaders(
    const network::mojom::URLResponseHead& response_head,
    blink::ServiceWorkerStatusCode* out_service_worker_status,
    network::URLLoaderCompletionStatus* out_completion_status,
    std::string* out_error_message);

bool ShouldBypassCacheDueToUpdateViaCache(
    bool is_main_script,
    blink::mojom::ServiceWorkerUpdateViaCache cache_mode);

bool ShouldValidateBrowserCacheForScript(
    bool is_main_script,
    bool force_bypass_cache,
    blink::mojom::ServiceWorkerUpdateViaCache cache_mode,
    base::TimeDelta time_since_last_check);

}  // namespace service_worker_loader_helpers

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_LOADER_HELPERS_H_
