// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_NAVIGATION_LOADER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_NAVIGATION_LOADER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_response_type.h"
#include "content/browser/url_loader_factory_getter.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_stream_handle.mojom.h"

namespace content {

class ServiceWorkerVersion;
class ServiceWorkerProviderHost;

// ServiceWorkerNavigationLoader is the URLLoader used for main resource
// requests (i.e., navigation and shared worker requests) that (potentially) go
// through a service worker. This loader is only used for the main resource
// request; once the response is delivered, the resulting client loads
// subresources via ServiceWorkerSubresourceLoader.
//
// This class is owned by ServiceWorkerControlleeRequestHandler until it is
// bound to a URLLoader request. After it is bound |this| is kept alive until
// the Mojo connection to this URLLoader is dropped.
class CONTENT_EXPORT ServiceWorkerNavigationLoader
    : public network::mojom::URLLoader {
 public:
  using ResponseType = ServiceWorkerResponseType;

  class CONTENT_EXPORT Delegate {
   public:
    virtual ~Delegate() {}

    // Returns the ServiceWorkerVersion fetch events for this request job should
    // be dispatched to. If no appropriate worker can be determined, returns
    // nullptr.
    virtual ServiceWorkerVersion* GetServiceWorkerVersion() = 0;

    // Called after dispatching the fetch event to determine if processing of
    // the request should still continue, or if processing should be aborted.
    // TODO(falken): This can probably be deleted.
    virtual bool RequestStillValid() = 0;

    // Called to signal that loading failed, and that the resource being loaded
    // was a main resource.
    virtual void MainResourceLoadFailed() = 0;
  };

  // Created by ServiceWorkerControlleeRequestHandler::MaybeCreateLoader
  // when starting to load a main resource.
  //
  // For the navigation case, this job typically works in the following order:
  // 1. One of the FallbackTo* or ForwardTo* methods are called by
  //    ServiceWorkerControlleeRequestHandler, which determines how the request
  //    should be served (e.g. should fallback to network or should be sent to
  //    the SW). If it decides to fallback to the network this will call
  //    |loader_callback| with a null RequestHandler, which will be then handled
  //    by NavigationURLLoaderImpl.
  // 2. If it is decided that the request should be sent to the SW,
  //    this job calls |loader_callback|, passing StartRequest as the
  //    RequestHandler.
  // 3. At this point, the NavigationURLLoaderImpl can throttle the request,
  //    and invoke the RequestHandler later with a possibly modified request.
  // 4. StartRequest is invoked. This dispatches a FetchEvent.
  // 5. DidDispatchFetchEvent() determines the request's final destination. If
  //    it turns out we need to fallback to network, it calls
  //    |fallback_callback|.
  // 6. Otherwise if the SW returned a stream or blob as a response
  //    this job passes the response to the network::mojom::URLLoaderClientPtr
  //    connected to NavigationURLLoaderImpl (for resource loading for
  //    navigation), that was given to StartRequest. This forwards the
  //    blob/stream data pipe to the NavigationURLLoader.
  //
  // Loads for shared workers work similarly, except SharedWorkerScriptLoader
  // is used instead of NavigationURLLoaderImpl.
  ServiceWorkerNavigationLoader(
      NavigationLoaderInterceptor::LoaderCallback loader_callback,
      NavigationLoaderInterceptor::FallbackCallback fallback_callback,
      Delegate* delegate,
      const network::ResourceRequest& tentative_resource_request,
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter);

  ~ServiceWorkerNavigationLoader() override;

  // Called via ServiceWorkerControlleeRequestHandler.
  void FallbackToNetwork();
  void ForwardToServiceWorker();
  bool ShouldFallbackToNetwork();
  bool ShouldForwardToServiceWorker();

  // The navigation request that was holding this job is
  // going away. Calling this internally calls |DeleteIfNeeded()|
  // and may delete |this| if it is not bound to a endpoint.
  // Otherwise |this| will be kept around as far as the loader
  // endpoint is held by the client.
  void DetachedFromRequest();

  base::WeakPtr<ServiceWorkerNavigationLoader> AsWeakPtr();

 private:
  class StreamWaiter;
  enum class Status {
    kNotStarted,
    // |binding_| is bound and the fetch event is being dispatched to the
    // service worker.
    kStarted,
    // The response head has been sent to |url_loader_client_|.
    kSentHeader,
    // The data pipe for the response body has been sent to
    // |url_loader_client_|. The body is being written to the pipe.
    kSentBody,
    // OnComplete() was called on |url_loader_client_|, or fallback to network
    // occurred so the request was not handled.
    kCompleted,
  };

  // For FORWARD_TO_SERVICE_WORKER case.
  void StartRequest(const network::ResourceRequest& resource_request,
                    network::mojom::URLLoaderRequest request,
                    network::mojom::URLLoaderClientPtr client);
  void DidPrepareFetchEvent(scoped_refptr<ServiceWorkerVersion> version,
                            EmbeddedWorkerStatus initial_worker_status);
  void DidDispatchFetchEvent(
      blink::ServiceWorkerStatusCode status,
      ServiceWorkerFetchDispatcher::FetchEventResult fetch_result,
      blink::mojom::FetchAPIResponsePtr response,
      blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
      blink::mojom::ServiceWorkerFetchEventTimingPtr timing,
      scoped_refptr<ServiceWorkerVersion> version);

  void StartResponse(blink::mojom::FetchAPIResponsePtr response,
                     scoped_refptr<ServiceWorkerVersion> version,
                     blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream);

  // Calls url_loader_client_->OnReceiveResponse() with |response_head_|.
  void CommitResponseHeaders();

  // Calls url_loader_client_->OnStartLoadingResponseBody() with
  // |response_body|.
  void CommitResponseBody(mojo::ScopedDataPipeConsumerHandle response_body);

  // Creates and sends an empty response's body with the net::OK status.
  // Sends net::ERR_INSUFFICIENT_RESOURCES when it can't be created.
  void CommitEmptyResponseAndComplete();

  // Calls url_loader_client_->OnComplete(). |reason| will be recorded as an
  // argument of TRACE_EVENT.
  void CommitCompleted(int error_code, const char* reason);

  // network::mojom::URLLoader:
  void FollowRedirect(const std::vector<std::string>& removed_headers,
                      const net::HttpRequestHeaders& modified_headers,
                      const base::Optional<GURL>& new_url) override;
  void ProceedWithResponse() override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  void OnBlobReadingComplete(int net_error);

  void OnConnectionClosed();
  void DeleteIfNeeded();

  // Records loading milestones. Called only after ForwardToServiceWorker() is
  // called and there was no error. |handled| is true when a fetch handler
  // handled the request (i.e. non network fallback case).
  void RecordTimingMetrics(bool handled);

  void TransitionToStatus(Status new_status);

  ResponseType response_type_ = ResponseType::NOT_DETERMINED;
  NavigationLoaderInterceptor::LoaderCallback loader_callback_;
  NavigationLoaderInterceptor::FallbackCallback fallback_callback_;

  // |delegate_| is non-null and owns |this| until DetachedFromRequest() is
  // called. Once that is called, |delegate_| is reset to null and |this| owns
  // itself, self-destructing when a connection error on |binding_| occurs.
  //
  // Note: A WeakPtr wouldn't be super safe here because the delegate can
  // conceivably still be alive and used for another loader, after calling
  // DetachedFromRequest() for this loader.
  Delegate* delegate_ = nullptr;

  network::ResourceRequest resource_request_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter_;
  std::unique_ptr<ServiceWorkerFetchDispatcher> fetch_dispatcher_;
  std::unique_ptr<StreamWaiter> stream_waiter_;
  // The blob needs to be held while it's read to keep it alive.
  blink::mojom::BlobPtr body_as_blob_;

  bool did_navigation_preload_ = false;
  network::ResourceResponseHead response_head_;

  bool devtools_attached_ = false;
  blink::mojom::ServiceWorkerFetchEventTimingPtr fetch_event_timing_;
  base::TimeTicks completion_time_;
  network::mojom::FetchResponseSource response_source_ =
      network::mojom::FetchResponseSource::kUnspecified;

  // Pointer to the URLLoaderClient (i.e. NavigationURLLoader).
  network::mojom::URLLoaderClientPtr url_loader_client_;
  mojo::Binding<network::mojom::URLLoader> binding_;

  Status status_ = Status::kNotStarted;

  base::WeakPtrFactory<ServiceWorkerNavigationLoader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerNavigationLoader);
};

// Owns a loader and calls DetachedFromRequest() to release it.
class ServiceWorkerNavigationLoaderWrapper {
 public:
  explicit ServiceWorkerNavigationLoaderWrapper(
      std::unique_ptr<ServiceWorkerNavigationLoader> loader);
  ~ServiceWorkerNavigationLoaderWrapper();

  ServiceWorkerNavigationLoader* get() { return loader_.get(); }

 private:
  std::unique_ptr<ServiceWorkerNavigationLoader> loader_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerNavigationLoaderWrapper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_NAVIGATION_LOADER_H_
