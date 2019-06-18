// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTROLLEE_REQUEST_HANDLER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTROLLEE_REQUEST_HANDLER_H_

#include <stdint.h>
#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/service_worker/service_worker_navigation_loader.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/resource_type.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "url/gurl.h"

namespace network {
class ResourceRequestBody;
}

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerProviderHost;
class ServiceWorkerRegistration;
class ServiceWorkerVersion;

// Handles main resource requests for service worker clients (documents and
// shared workers).
// TODO(falken): Rename to ServiceWorkerNavigationLoaderInterceptor.
class CONTENT_EXPORT ServiceWorkerControlleeRequestHandler final
    : public NavigationLoaderInterceptor,
      public ServiceWorkerNavigationLoader::Delegate {
 public:
  ServiceWorkerControlleeRequestHandler(
      base::WeakPtr<ServiceWorkerContextCore> context,
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      network::mojom::RequestMode request_mode,
      network::mojom::CredentialsMode credentials_mode,
      network::mojom::RedirectMode redirect_mode,
      const std::string& integrity,
      bool keepalive,
      ResourceType resource_type,
      blink::mojom::RequestContextType request_context_type,
      scoped_refptr<network::ResourceRequestBody> body);
  ~ServiceWorkerControlleeRequestHandler() override;

  // NavigationLoaderInterceptor overrides:

  // This could get called multiple times during the lifetime in redirect
  // cases. (In fallback-to-network cases we basically forward the request
  // to the request to the next request handler)
  void MaybeCreateLoader(const network::ResourceRequest& tentative_request,
                         ResourceContext* resource_context,
                         LoaderCallback callback,
                         FallbackCallback fallback_callback) override;
  // Returns params with the ControllerServiceWorkerPtr if we have found
  // a matching controller service worker for the |request| that is given
  // to MaybeCreateLoader(). Otherwise this returns base::nullopt.
  base::Optional<SubresourceLoaderParams> MaybeCreateSubresourceLoaderParams()
      override;

  // Exposed for testing.
  ServiceWorkerNavigationLoader* loader() {
    return loader_wrapper_ ? loader_wrapper_->get() : nullptr;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerControlleeRequestHandlerTest,
                           ActivateWaitingVersion);
  class ScopedDisallowSetControllerRegistration;

  // TODO(falken): Remove the "MainResource" names, they are redundant as this
  // handler is for main resources only.
  void PrepareForMainResource(const GURL& url, const GURL& site_for_cookies);
  void DidLookupRegistrationForMainResource(
      std::unique_ptr<ScopedDisallowSetControllerRegistration>
          disallow_controller,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void ContinueWithInScopeMainResourceRequest(
      scoped_refptr<ServiceWorkerRegistration> registration,
      scoped_refptr<ServiceWorkerVersion> version,
      std::unique_ptr<ScopedDisallowSetControllerRegistration>
          disallow_controller);

  // For forced update.
  void DidUpdateRegistration(
      scoped_refptr<ServiceWorkerRegistration> original_registration,
      std::unique_ptr<ScopedDisallowSetControllerRegistration>
          disallow_controller,
      blink::ServiceWorkerStatusCode status,
      const std::string& status_message,
      int64_t registration_id);
  void OnUpdatedVersionStatusChanged(
      scoped_refptr<ServiceWorkerRegistration> registration,
      scoped_refptr<ServiceWorkerVersion> version,
      std::unique_ptr<ScopedDisallowSetControllerRegistration>
          disallow_controller);

  // ServiceWorkerNavigationLoader::Delegate implementation:
  ServiceWorkerVersion* GetServiceWorkerVersion() override;
  bool RequestStillValid() override;
  void MainResourceLoadFailed() override;

  // Sets |job_| to nullptr, and clears all extra response info associated with
  // that job, except for timing information.
  void ClearJob();

  // Schedules a service worker update to occur shortly after the page and its
  // initial subresources load, if this handler was for a navigation.
  void MaybeScheduleUpdate();

  const base::WeakPtr<ServiceWorkerContextCore> context_;
  const base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  const ResourceType resource_type_;
  std::unique_ptr<ServiceWorkerNavigationLoaderWrapper> loader_wrapper_;
  network::mojom::RequestMode request_mode_;
  network::mojom::CredentialsMode credentials_mode_;
  network::mojom::RedirectMode redirect_mode_;
  std::string integrity_;
  const bool keepalive_;
  blink::mojom::RequestContextType request_context_type_;
  scoped_refptr<network::ResourceRequestBody> body_;
  ResourceContext* resource_context_;
  GURL stripped_url_;
  bool force_update_started_;
  base::TimeTicks registration_lookup_start_time_;

  base::WeakPtrFactory<ServiceWorkerControlleeRequestHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerControlleeRequestHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTROLLEE_REQUEST_HANDLER_H_
