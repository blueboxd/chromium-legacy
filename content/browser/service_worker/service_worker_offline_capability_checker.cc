// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_offline_capability_checker.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/guid.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl.h"
#include "url/gurl.h"

namespace content {

ServiceWorkerOfflineCapabilityChecker::ServiceWorkerOfflineCapabilityChecker(
    const GURL& url)
    : url_(url) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
}

ServiceWorkerOfflineCapabilityChecker::
    ~ServiceWorkerOfflineCapabilityChecker() = default;

void ServiceWorkerOfflineCapabilityChecker::Start(
    ServiceWorkerRegistry* registry,
    ServiceWorkerContext::CheckOfflineCapabilityCallback callback) {
  callback_ = std::move(callback);
  registry->FindRegistrationForClientUrl(
      url_, base::BindOnce(
                &ServiceWorkerOfflineCapabilityChecker::DidFindRegistration,
                // We can use base::Unretained(this) because |this| is expected
                // to be alive until the |callback_| is called.
                base::Unretained(this)));
}

void ServiceWorkerOfflineCapabilityChecker::DidFindRegistration(
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback_).Run(OfflineCapability::kUnsupported);
    return;
  }

  if (registration->is_uninstalling() || registration->is_uninstalled()) {
    std::move(callback_).Run(OfflineCapability::kUnsupported);
    return;
  }

  scoped_refptr<ServiceWorkerVersion> preferred_version =
      registration->active_version();
  if (!preferred_version) {
    preferred_version = registration->waiting_version();
  }
  if (!preferred_version) {
    std::move(callback_).Run(OfflineCapability::kUnsupported);
    return;
  }

  ServiceWorkerVersion::FetchHandlerExistence existence =
      preferred_version->fetch_handler_existence();

  DCHECK_NE(existence, ServiceWorkerVersion::FetchHandlerExistence::UNKNOWN);

  if (existence != ServiceWorkerVersion::FetchHandlerExistence::EXISTS) {
    std::move(callback_).Run(OfflineCapability::kUnsupported);
    return;
  }

  if (preferred_version->status() != ServiceWorkerVersion::Status::ACTIVATING &&
      preferred_version->status() != ServiceWorkerVersion::Status::ACTIVATED) {
    // ServiceWorkerFetchDispatcher assumes that the version's status is
    // ACTIVATING or ACTIVATED. If the version's status is other one, we return
    // kUnsupported, without waiting its status becoming ACTIVATING because that
    // is not always guaranteed.
    // TODO(hayato): We can do a bit better, such as 1) trigger the activation
    // and wait, or 2) return a value to indicate the service worker is
    // installed but not yet activated.
    std::move(callback_).Run(OfflineCapability::kUnsupported);
    return;
  }

  auto request = blink::mojom::FetchAPIRequest::New();
  request->url = url_;
  request->method = "GET";
  request->is_main_resource_load = true;

  fetch_dispatcher_ = std::make_unique<ServiceWorkerFetchDispatcher>(
      std::move(request), blink::mojom::ResourceType::kMainFrame,
      base::GenerateGUID() /* client_id */, std::move(preferred_version),
      base::DoNothing() /* prepare callback */,
      base::BindOnce(&ServiceWorkerOfflineCapabilityChecker::OnFetchResult,
                     base::Unretained(this)),
      /*is_offline_capability_check=*/true);
  fetch_dispatcher_->Run();
}

void ServiceWorkerOfflineCapabilityChecker::OnFetchResult(
    blink::ServiceWorkerStatusCode status,
    ServiceWorkerFetchDispatcher::FetchEventResult result,
    blink::mojom::FetchAPIResponsePtr response,
    blink::mojom::ServiceWorkerStreamHandlePtr /* stream */,
    blink::mojom::ServiceWorkerFetchEventTimingPtr /* timing */,
    scoped_refptr<ServiceWorkerVersion>) {
  if (status == blink::ServiceWorkerStatusCode::kOk &&
      result == ServiceWorkerFetchDispatcher::FetchEventResult::kGotResponse &&
      // TODO(hayato): Investigate whether any 2xx should be accepted or not.
      response->status_code == 200) {
    std::move(callback_).Run(OfflineCapability::kSupported);
  } else {
    // TODO(hayato): At present, we return kUnsupported even if the detection
    // failed due to internal errors (disk fialures, timeout, etc). In the
    // future, we might want to return another enum value so that the callside
    // can know whether internal errors happened or not.
    std::move(callback_).Run(OfflineCapability::kUnsupported);
  }
}

}  // namespace content
