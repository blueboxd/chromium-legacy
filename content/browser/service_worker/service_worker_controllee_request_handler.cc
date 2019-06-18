// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_controllee_request_handler.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/trace_event/trace_event.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_navigation_loader.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/resource_response_info.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "components/offline_pages/core/request_header/offline_page_header.h"
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

namespace content {

namespace {

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
// A web page, regardless of whether the service worker is used or not, could
// be downloaded with the offline snapshot captured. The user can then open
// the downloaded page which is identified by the presence of a specific
// offline header in the network request. In this case, we want to fall back
// in order for the subsequent offline page interceptor to bring up the
// offline snapshot of the page.
bool ShouldFallbackToLoadOfflinePage(
    const net::HttpRequestHeaders& extra_request_headers) {
  std::string offline_header_value;
  if (!extra_request_headers.GetHeader(offline_pages::kOfflinePageHeader,
                                       &offline_header_value)) {
    return false;
  }
  offline_pages::OfflinePageHeader offline_header(offline_header_value);
  return offline_header.reason !=
             offline_pages::OfflinePageHeader::Reason::NONE &&
         offline_header.reason !=
             offline_pages::OfflinePageHeader::Reason::RELOAD;
}
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

}  // namespace

// RAII class that disallows calling SetControllerRegistration() on a provider
// host.
class ServiceWorkerControlleeRequestHandler::
    ScopedDisallowSetControllerRegistration {
 public:
  explicit ScopedDisallowSetControllerRegistration(
      base::WeakPtr<ServiceWorkerProviderHost> provider_host)
      : provider_host_(std::move(provider_host)) {
    DCHECK(provider_host_->IsSetControllerRegistrationAllowed())
        << "The host already disallows using a registration; nested disallow "
           "is not supported.";
    provider_host_->AllowSetControllerRegistration(false);
  }

  ~ScopedDisallowSetControllerRegistration() {
    if (!provider_host_)
      return;
    DCHECK(!provider_host_->IsSetControllerRegistrationAllowed())
        << "Failed to disallow using a registration.";
    provider_host_->AllowSetControllerRegistration(true);
  }

 private:
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;

  DISALLOW_COPY_AND_ASSIGN(ScopedDisallowSetControllerRegistration);
};

ServiceWorkerControlleeRequestHandler::ServiceWorkerControlleeRequestHandler(
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    network::mojom::RequestMode request_mode,
    network::mojom::CredentialsMode credentials_mode,
    network::mojom::RedirectMode redirect_mode,
    const std::string& integrity,
    bool keepalive,
    ResourceType resource_type,
    blink::mojom::RequestContextType request_context_type,
    scoped_refptr<network::ResourceRequestBody> body)
    : context_(std::move(context)),
      provider_host_(std::move(provider_host)),
      resource_type_(resource_type),
      request_mode_(request_mode),
      credentials_mode_(credentials_mode),
      redirect_mode_(redirect_mode),
      integrity_(integrity),
      keepalive_(keepalive),
      request_context_type_(request_context_type),
      body_(std::move(body)),
      force_update_started_(false),
      weak_factory_(this) {
  DCHECK(ServiceWorkerUtils::IsMainResourceType(resource_type));
}

ServiceWorkerControlleeRequestHandler::
    ~ServiceWorkerControlleeRequestHandler() {
  MaybeScheduleUpdate();
}

void ServiceWorkerControlleeRequestHandler::MaybeScheduleUpdate() {
  if (!provider_host_ || !provider_host_->controller())
    return;

  // For navigations, the update logic is taken care of
  // during navigation and waits for the HintToUpdateServiceWorker message.
  if (IsResourceTypeFrame(resource_type_))
    return;

  // For shared workers. The renderer doesn't yet send a
  // HintToUpdateServiceWorker message.
  // TODO(falken): Make the renderer send the message for shared worker,
  // to simplify the code.

  // If DevTools forced an update, there is no need to update again.
  if (force_update_started_)
    return;

  provider_host_->controller()->ScheduleUpdate();
}

void ServiceWorkerControlleeRequestHandler::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    ResourceContext* resource_context,
    LoaderCallback callback,
    FallbackCallback fallback_callback) {
  ClearJob();

  if (!context_ || !provider_host_) {
    // We can't do anything other than to fall back to network.
    std::move(callback).Run({});
    return;
  }

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  // Fall back for the subsequent offline page interceptor to load the offline
  // snapshot of the page if required.
  //
  // TODO(crbug.com/876527): Figure out how offline page interception should
  // interact with URLLoaderThrottles. It might be incorrect to use
  // |tentative_resource_request.headers| here, since throttles can rewrite
  // headers between now and when the request handler passed to
  // |loader_callback_| is invoked.
  if (ShouldFallbackToLoadOfflinePage(tentative_resource_request.headers)) {
    std::move(callback).Run({});
    return;
  }
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

  loader_wrapper_ = std::make_unique<ServiceWorkerNavigationLoaderWrapper>(
      std::make_unique<ServiceWorkerNavigationLoader>(
          std::move(callback), std::move(fallback_callback), this,
          tentative_resource_request, provider_host_,
          base::WrapRefCounted(context_->loader_factory_getter())));

  resource_context_ = resource_context;

  PrepareForMainResource(tentative_resource_request.url,
                         tentative_resource_request.site_for_cookies);

  if (loader()->ShouldFallbackToNetwork()) {
    // The job already fell back to network. Clear the job now.
    ClearJob();
    return;
  }

  // We will asynchronously continue on DidLookupRegistrationForMainResource.
}

base::Optional<SubresourceLoaderParams>
ServiceWorkerControlleeRequestHandler::MaybeCreateSubresourceLoaderParams() {
  // We didn't create URLLoader for this request.
  if (!loader())
    return base::nullopt;

  // DidLookupRegistrationForMainResource() for the request didn't find
  // a matching service worker for this request, and
  // ServiceWorkerProviderHost::SetControllerRegistration() was not called.
  if (!provider_host_ || !provider_host_->controller())
    return base::nullopt;

  // Otherwise let's send the controller service worker information along
  // with the navigation commit.
  SubresourceLoaderParams params;
  auto controller_info = blink::mojom::ControllerServiceWorkerInfo::New();
  controller_info->mode = provider_host_->GetControllerMode();
  // Note that |controller_info->endpoint| is null if the controller has no
  // fetch event handler. In that case the renderer frame won't get the
  // controller pointer upon the navigation commit, and subresource loading will
  // not be intercepted. (It might get intercepted later if the controller
  // changes due to skipWaiting() so SetController is sent.)
  controller_info->endpoint =
      provider_host_->GetControllerServiceWorkerPtr().PassInterface();
  controller_info->client_id = provider_host_->client_uuid();
  if (provider_host_->fetch_request_window_id()) {
    controller_info->fetch_request_window_id =
        base::make_optional(provider_host_->fetch_request_window_id());
  }
  base::WeakPtr<ServiceWorkerObjectHost> object_host =
      provider_host_->GetOrCreateServiceWorkerObjectHost(
          provider_host_->controller());
  if (object_host) {
    params.controller_service_worker_object_host = object_host;
    controller_info->object_info = object_host->CreateIncompleteObjectInfo();
  }
  for (const auto feature : provider_host_->controller()->used_features()) {
    controller_info->used_features.push_back(feature);
  }
  params.controller_service_worker_info = std::move(controller_info);
  return base::Optional<SubresourceLoaderParams>(std::move(params));
}

void ServiceWorkerControlleeRequestHandler::PrepareForMainResource(
    const GURL& url,
    const GURL& site_for_cookies) {
  DCHECK(loader());
  DCHECK(context_);
  DCHECK(provider_host_);
  TRACE_EVENT_ASYNC_BEGIN1(
      "ServiceWorker",
      "ServiceWorkerControlleeRequestHandler::PrepareForMainResource", this,
      "URL", url.spec());
  // The provider host may already have set a controller in redirect case,
  // unset it now.
  provider_host_->SetControllerRegistration(
      nullptr, false /* notify_controllerchange */);

  // Also prevent a registration from claiming this host while it's not
  // yet execution ready.
  auto disallow_controller =
      std::make_unique<ScopedDisallowSetControllerRegistration>(provider_host_);

  stripped_url_ = net::SimplifyUrlForRequest(url);
  provider_host_->UpdateUrls(stripped_url_, site_for_cookies);
  registration_lookup_start_time_ = base::TimeTicks::Now();
  context_->storage()->FindRegistrationForDocument(
      stripped_url_, base::BindOnce(&ServiceWorkerControlleeRequestHandler::
                                        DidLookupRegistrationForMainResource,
                                    weak_factory_.GetWeakPtr(),
                                    std::move(disallow_controller)));
}

void ServiceWorkerControlleeRequestHandler::
    DidLookupRegistrationForMainResource(
        std::unique_ptr<ScopedDisallowSetControllerRegistration>
            disallow_controller,
        blink::ServiceWorkerStatusCode status,
        scoped_refptr<ServiceWorkerRegistration> registration) {
  // The job may have been destroyed before this was invoked.
  if (!loader())
    return;

  ServiceWorkerMetrics::RecordLookupRegistrationTime(
      status, base::TimeTicks::Now() - registration_lookup_start_time_);

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    loader()->FallbackToNetwork();
    TRACE_EVENT_ASYNC_END1(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::PrepareForMainResource", this,
        "Status", blink::ServiceWorkerStatusToString(status));
    return;
  }
  DCHECK(registration);

  if (!provider_host_) {
    loader()->FallbackToNetwork();
    TRACE_EVENT_ASYNC_END1(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::PrepareForMainResource", this,
        "Info", "No Provider");
    return;
  }
  provider_host_->AddMatchingRegistration(registration.get());

  if (!context_) {
    loader()->FallbackToNetwork();
    TRACE_EVENT_ASYNC_END1(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::PrepareForMainResource", this,
        "Info", "No Context");
    return;
  }

  if (!GetContentClient()->browser()->AllowServiceWorker(
          registration->scope(), provider_host_->site_for_cookies(),
          resource_context_, provider_host_->web_contents_getter())) {
    loader()->FallbackToNetwork();
    TRACE_EVENT_ASYNC_END1(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::PrepareForMainResource", this,
        "Info", "ServiceWorker is blocked");
    return;
  }

  if (!provider_host_->IsContextSecureForServiceWorker()) {
    // TODO(falken): Figure out a way to surface in the page's DevTools
    // console that the service worker was blocked for security.
    loader()->FallbackToNetwork();
    TRACE_EVENT_ASYNC_END1(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::PrepareForMainResource", this,
        "Info", "Insecure context");
    return;
  }

  const bool need_to_update =
      !force_update_started_ && context_->force_update_on_page_load();
  if (need_to_update) {
    force_update_started_ = true;
    context_->UpdateServiceWorker(
        registration.get(), true /* force_bypass_cache */,
        true /* skip_script_comparison */,
        base::BindOnce(
            &ServiceWorkerControlleeRequestHandler::DidUpdateRegistration,
            weak_factory_.GetWeakPtr(), registration,
            std::move(disallow_controller)));
    return;
  }

  // Initiate activation of a waiting version. Usually a register job initiates
  // activation but that doesn't happen if the browser exits prior to activation
  // having occurred. This check handles that case.
  if (registration->waiting_version())
    registration->ActivateWaitingVersionWhenReady();

  scoped_refptr<ServiceWorkerVersion> active_version =
      registration->active_version();
  if (!active_version) {
    loader()->FallbackToNetwork();
    TRACE_EVENT_ASYNC_END1(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::PrepareForMainResource", this,
        "Info", "No active version, so falling back to network");
    return;
  }

  DCHECK(active_version->status() == ServiceWorkerVersion::ACTIVATING ||
         active_version->status() == ServiceWorkerVersion::ACTIVATED)
      << ServiceWorkerVersion::VersionStatusToString(active_version->status());
  // Wait until it's activated before firing fetch events.
  if (active_version->status() == ServiceWorkerVersion::ACTIVATING) {
    registration->active_version()->RegisterStatusChangeCallback(
        base::BindOnce(&ServiceWorkerControlleeRequestHandler::
                           ContinueWithInScopeMainResourceRequest,
                       weak_factory_.GetWeakPtr(), registration, active_version,
                       std::move(disallow_controller)));
    TRACE_EVENT_ASYNC_END1(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::PrepareForMainResource", this,
        "Info", "Wait until finished SW activation");
    return;
  }

  ContinueWithInScopeMainResourceRequest(std::move(registration),
                                         std::move(active_version),
                                         std::move(disallow_controller));
}

void ServiceWorkerControlleeRequestHandler::
    ContinueWithInScopeMainResourceRequest(
        scoped_refptr<ServiceWorkerRegistration> registration,
        scoped_refptr<ServiceWorkerVersion> active_version,
        std::unique_ptr<ScopedDisallowSetControllerRegistration>
            disallow_controller) {
  // The job may have been destroyed before this was invoked. In that
  // case, |loader()| can't be used, so return.
  if (!loader()) {
    TRACE_EVENT_ASYNC_END1(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::PrepareForMainResource", this,
        "Info", "The job was destroyed");
    return;
  }

  if (!provider_host_) {
    loader()->FallbackToNetwork();
    TRACE_EVENT_ASYNC_END1(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::PrepareForMainResource", this,
        "Info", "The provider host is gone, so falling back to network");
    return;
  }

  if (active_version->status() != ServiceWorkerVersion::ACTIVATED) {
    // TODO(falken): Clean this up and clarify in what cases we come here. I
    // guess it's:
    // - strange system error cases where promoting from ACTIVATING to ACTIVATED
    //   failed (shouldn't happen)
    // - something calling Doom(), etc, making the active_version REDUNDANT
    // - a version called skipWaiting() during activation so the expected
    //   version is no longer the active one (shouldn't happen: skipWaiting()
    //   waits for the active version to finish activating).
    // In most cases, it sounds like falling back to network would not be right,
    // since it's still in-scope. We probably should do:
    //   1) If the provider host has an active version that is ACTIVATED, just
    //      use that, even if it wasn't the expected one.
    //   2) If the provider host has an active version that is not ACTIVATED,
    //      just fail the load. The correct thing is probably to re-try
    //      activating that version, but there's a risk of an infinite loop of
    //      retries.
    //   3) If the provider host does not have an active version, just fail the
    //      load.
    loader()->FallbackToNetwork();
    TRACE_EVENT_ASYNC_END2(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::PrepareForMainResource", this,
        "Info",
        "The expected active version is not ACTIVATED, so falling back to "
        "network",
        "Status",
        ServiceWorkerVersion::VersionStatusToString(active_version->status()));
    return;
  }

  disallow_controller.reset();
  provider_host_->SetControllerRegistration(
      registration, false /* notify_controllerchange */);

  DCHECK_EQ(active_version, registration->active_version());
  DCHECK_EQ(active_version, provider_host_->controller());
  DCHECK_NE(active_version->fetch_handler_existence(),
            ServiceWorkerVersion::FetchHandlerExistence::UNKNOWN);
  ServiceWorkerMetrics::CountControlledPageLoad(
      active_version->site_for_uma(), stripped_url_,
      resource_type_ == ResourceType::kMainFrame);

  if (IsResourceTypeFrame(resource_type_))
    provider_host_->AddServiceWorkerToUpdate(active_version);

  bool should_forward = active_version->fetch_handler_existence() ==
                        ServiceWorkerVersion::FetchHandlerExistence::EXISTS;
  if (should_forward)
    loader()->ForwardToServiceWorker();
  else
    loader()->FallbackToNetwork();

  TRACE_EVENT_ASYNC_END1(
      "ServiceWorker",
      "ServiceWorkerControlleeRequestHandler::PrepareForMainResource", this,
      "Info",
      (should_forward)
          ? "Forwarded to the ServiceWorker"
          : "Skipped the ServiceWorker which has no fetch handler");
}

void ServiceWorkerControlleeRequestHandler::DidUpdateRegistration(
    scoped_refptr<ServiceWorkerRegistration> original_registration,
    std::unique_ptr<ScopedDisallowSetControllerRegistration>
        disallow_controller,
    blink::ServiceWorkerStatusCode status,
    const std::string& status_message,
    int64_t registration_id) {
  DCHECK(force_update_started_);

  // The job may have been destroyed before this was invoked.
  if (!loader())
    return;

  if (!context_) {
    loader()->FallbackToNetwork();
    return;
  }
  if (status != blink::ServiceWorkerStatusCode::kOk ||
      !original_registration->installing_version()) {
    // Update failed. Look up the registration again since the original
    // registration was possibly unregistered in the meantime.
    context_->storage()->FindRegistrationForDocument(
        stripped_url_, base::BindOnce(&ServiceWorkerControlleeRequestHandler::
                                          DidLookupRegistrationForMainResource,
                                      weak_factory_.GetWeakPtr(),
                                      std::move(disallow_controller)));
    return;
  }
  DCHECK_EQ(original_registration->id(), registration_id);
  ServiceWorkerVersion* new_version =
      original_registration->installing_version();
  new_version->ReportForceUpdateToDevTools();
  new_version->set_skip_waiting(true);
  new_version->RegisterStatusChangeCallback(base::BindOnce(
      &ServiceWorkerControlleeRequestHandler::OnUpdatedVersionStatusChanged,
      weak_factory_.GetWeakPtr(), std::move(original_registration),
      base::WrapRefCounted(new_version), std::move(disallow_controller)));
}

void ServiceWorkerControlleeRequestHandler::OnUpdatedVersionStatusChanged(
    scoped_refptr<ServiceWorkerRegistration> registration,
    scoped_refptr<ServiceWorkerVersion> version,
    std::unique_ptr<ScopedDisallowSetControllerRegistration>
        disallow_controller) {
  // The job may have been destroyed before this was invoked.
  if (!loader())
    return;

  if (!context_) {
    loader()->FallbackToNetwork();
    return;
  }
  if (version->status() == ServiceWorkerVersion::ACTIVATED ||
      version->status() == ServiceWorkerVersion::REDUNDANT) {
    // When the status is REDUNDANT, the update failed (eg: script error), we
    // continue with the incumbent version.
    // In case unregister job may have run, look up the registration again.
    context_->storage()->FindRegistrationForDocument(
        stripped_url_, base::BindOnce(&ServiceWorkerControlleeRequestHandler::
                                          DidLookupRegistrationForMainResource,
                                      weak_factory_.GetWeakPtr(),
                                      std::move(disallow_controller)));
    return;
  }
  version->RegisterStatusChangeCallback(base::BindOnce(
      &ServiceWorkerControlleeRequestHandler::OnUpdatedVersionStatusChanged,
      weak_factory_.GetWeakPtr(), std::move(registration), version,
      std::move(disallow_controller)));
}

ServiceWorkerVersion*
ServiceWorkerControlleeRequestHandler::GetServiceWorkerVersion() {
  if (!provider_host_)
    return nullptr;
  return provider_host_->controller();
}

bool ServiceWorkerControlleeRequestHandler::RequestStillValid() {
  // A null |provider_host_| probably means the tab was closed. The null value
  // would cause problems down the line, so bail out.
  return !!provider_host_;
}

void ServiceWorkerControlleeRequestHandler::MainResourceLoadFailed() {
  DCHECK(provider_host_);
  // Detach the controller so subresource requests also skip the worker.
  provider_host_->NotifyControllerLost();
}

void ServiceWorkerControlleeRequestHandler::ClearJob() {
  // Invalidate weak pointers to cancel RegisterStatusChangeCallback().
  // Otherwise we may end up calling ForwardToServiceWorer()
  // or FallbackToNetwork() twice on the same |loader()|.
  // TODO(bashi): Consider not to reuse this handler when restarting the
  // request after S13nServiceWorker is shipped.
  weak_factory_.InvalidateWeakPtrs();
  loader_wrapper_.reset();
}

}  // namespace content
