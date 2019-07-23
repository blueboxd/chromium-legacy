// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/prefetch_url_loader.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache_adapter.h"
#include "content/browser/web_package/signed_exchange_prefetch_handler.h"
#include "content/browser/web_package/signed_exchange_prefetch_metric_recorder.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/public/common/content_features.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/loader_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace content {

namespace {

constexpr char kSignedExchangeEnabledAcceptHeaderForPrefetch[] =
    "application/signed-exchange;v=b3;q=0.9,*/*;q=0.8";

}  // namespace

PrefetchURLLoader::PrefetchURLLoader(
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    base::RepeatingCallback<int(void)> frame_tree_node_id_getter,
    const network::ResourceRequest& resource_request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory,
    URLLoaderThrottlesGetter url_loader_throttles_getter,
    BrowserContext* browser_context,
    ResourceContext* resource_context,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    scoped_refptr<SignedExchangePrefetchMetricRecorder>
        signed_exchange_prefetch_metric_recorder,
    scoped_refptr<PrefetchedSignedExchangeCache>
        prefetched_signed_exchange_cache,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
    const std::string& accept_langs)
    : frame_tree_node_id_getter_(frame_tree_node_id_getter),
      resource_request_(resource_request),
      network_loader_factory_(std::move(network_loader_factory)),
      client_binding_(this),
      forwarding_client_(std::move(client)),
      url_loader_throttles_getter_(url_loader_throttles_getter),
      browser_context_(browser_context),
      resource_context_(resource_context),
      request_context_getter_(std::move(request_context_getter)),
      signed_exchange_prefetch_metric_recorder_(
          std::move(signed_exchange_prefetch_metric_recorder)),
      accept_langs_(accept_langs) {
  DCHECK(network_loader_factory_);

  if (IsSignedExchangeHandlingEnabled()) {
    // Set the SignedExchange accept header.
    // (https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#internet-media-type-applicationsigned-exchange).
    resource_request_.headers.SetHeader(
        network::kAcceptHeader, kSignedExchangeEnabledAcceptHeaderForPrefetch);
    if (prefetched_signed_exchange_cache &&
        resource_request.is_signed_exchange_prefetch_cache_enabled) {
      BrowserContext::BlobContextGetter getter;
      if (NavigationURLLoaderImpl::IsNavigationLoaderOnUIEnabled()) {
        getter = BrowserContext::GetBlobStorageContext(browser_context_);
      } else {
        getter = base::BindRepeating(
            [](base::WeakPtr<storage::BlobStorageContext> context) {
              return context;
            },
            std::move(blob_storage_context));
      }
      prefetched_signed_exchange_cache_adapter_ =
          std::make_unique<PrefetchedSignedExchangeCacheAdapter>(
              std::move(prefetched_signed_exchange_cache), std::move(getter),
              resource_request.url, this);
    }
  }

  network::mojom::URLLoaderClientPtr network_client;
  client_binding_.Bind(mojo::MakeRequest(&network_client));
  client_binding_.set_connection_error_handler(base::BindOnce(
      &PrefetchURLLoader::OnNetworkConnectionError, base::Unretained(this)));
  network_loader_factory_->CreateLoaderAndStart(
      mojo::MakeRequest(&loader_), routing_id, request_id, options,
      resource_request_, std::move(network_client), traffic_annotation);
}

PrefetchURLLoader::~PrefetchURLLoader() = default;

void PrefetchURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const base::Optional<GURL>& new_url) {
  DCHECK(modified_headers.IsEmpty())
      << "Redirect with modified headers was not supported yet. "
         "crbug.com/845683";
  DCHECK(!new_url) << "Redirect with modified URL was not "
                      "supported yet. crbug.com/845683";
  if (signed_exchange_prefetch_handler_) {
    // Rebind |client_binding_| and |loader_|.
    client_binding_.Bind(signed_exchange_prefetch_handler_->FollowRedirect(
        mojo::MakeRequest(&loader_)));
    return;
  }

  DCHECK(loader_);
  loader_->FollowRedirect(removed_headers,
                          net::HttpRequestHeaders() /* modified_headers */,
                          base::nullopt);
}

void PrefetchURLLoader::ProceedWithResponse() {
  if (loader_)
    loader_->ProceedWithResponse();
}

void PrefetchURLLoader::SetPriority(net::RequestPriority priority,
                                    int intra_priority_value) {
  if (loader_)
    loader_->SetPriority(priority, intra_priority_value);
}

void PrefetchURLLoader::PauseReadingBodyFromNet() {
  // TODO(kinuko): Propagate or handle the case where |loader_| is
  // detached (for SignedExchanges), see OnReceiveResponse.
  if (loader_)
    loader_->PauseReadingBodyFromNet();
}

void PrefetchURLLoader::ResumeReadingBodyFromNet() {
  // TODO(kinuko): Propagate or handle the case where |loader_| is
  // detached (for SignedExchanges), see OnReceiveResponse.
  if (loader_)
    loader_->ResumeReadingBodyFromNet();
}

void PrefetchURLLoader::OnReceiveResponse(
    const network::ResourceResponseHead& response) {
  if (IsSignedExchangeHandlingEnabled() &&
      signed_exchange_utils::ShouldHandleAsSignedHTTPExchange(
          resource_request_.url, response)) {
    DCHECK(!signed_exchange_prefetch_handler_);
    if (prefetched_signed_exchange_cache_adapter_) {
      prefetched_signed_exchange_cache_adapter_->OnReceiveOuterResponse(
          response);
    }
    // Note that after this point this doesn't directly get upcalls from the
    // network. (Until |this| calls the handler's FollowRedirect.)
    signed_exchange_prefetch_handler_ =
        std::make_unique<SignedExchangePrefetchHandler>(
            frame_tree_node_id_getter_, resource_request_, response,
            mojo::ScopedDataPipeConsumerHandle(), std::move(loader_),
            client_binding_.Unbind(), network_loader_factory_,
            url_loader_throttles_getter_, resource_context_,
            request_context_getter_, this,
            signed_exchange_prefetch_metric_recorder_, accept_langs_);
    return;
  }
  if (prefetched_signed_exchange_cache_adapter_ &&
      signed_exchange_prefetch_handler_) {
    prefetched_signed_exchange_cache_adapter_->OnReceiveInnerResponse(response);
  }

  forwarding_client_->OnReceiveResponse(response);
}

void PrefetchURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& head) {
  if (prefetched_signed_exchange_cache_adapter_ &&
      signed_exchange_prefetch_handler_) {
    prefetched_signed_exchange_cache_adapter_->OnReceiveRedirect(
        redirect_info.new_url,
        signed_exchange_prefetch_handler_->ComputeHeaderIntegrity(),
        signed_exchange_prefetch_handler_->GetSignatureExpireTime());
  }

  resource_request_.url = redirect_info.new_url;
  resource_request_.site_for_cookies = redirect_info.new_site_for_cookies;
  resource_request_.top_frame_origin = redirect_info.new_top_frame_origin;
  resource_request_.referrer = GURL(redirect_info.new_referrer);
  resource_request_.referrer_policy = redirect_info.new_referrer_policy;
  forwarding_client_->OnReceiveRedirect(redirect_info, head);
}

void PrefetchURLLoader::OnUploadProgress(int64_t current_position,
                                         int64_t total_size,
                                         base::OnceCallback<void()> callback) {
  forwarding_client_->OnUploadProgress(current_position, total_size,
                                       std::move(callback));
}

void PrefetchURLLoader::OnReceiveCachedMetadata(mojo_base::BigBuffer data) {
  // Just drop this; we don't need to forward this to the renderer
  // for prefetch.
}

void PrefetchURLLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void PrefetchURLLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  if (prefetched_signed_exchange_cache_adapter_ &&
      signed_exchange_prefetch_handler_) {
    prefetched_signed_exchange_cache_adapter_->OnStartLoadingResponseBody(
        std::move(body));
    return;
  }

  // Just drain the original response's body here.
  DCHECK(!pipe_drainer_);
  pipe_drainer_ =
      std::make_unique<mojo::DataPipeDrainer>(this, std::move(body));

  SendEmptyBody();
}

void PrefetchURLLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  if (prefetched_signed_exchange_cache_adapter_ &&
      signed_exchange_prefetch_handler_) {
    prefetched_signed_exchange_cache_adapter_->OnComplete(status);
    return;
  }

  SendOnComplete(status);
}

bool PrefetchURLLoader::SendEmptyBody() {
  // Send an empty response's body.
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  if (CreateDataPipe(nullptr, &producer, &consumer) != MOJO_RESULT_OK) {
    // No more resources available for creating a data pipe. Close the
    // connection, which will in turn make this loader destroyed.
    forwarding_client_->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
    forwarding_client_.reset();
    client_binding_.Close();
    return false;
  }
  forwarding_client_->OnStartLoadingResponseBody(std::move(consumer));
  return true;
}

void PrefetchURLLoader::SendOnComplete(
    const network::URLLoaderCompletionStatus& completion_status) {
  forwarding_client_->OnComplete(completion_status);
}

void PrefetchURLLoader::OnNetworkConnectionError() {
  // The network loader has an error; we should let the client know it's closed
  // by dropping this, which will in turn make this loader destroyed.
  forwarding_client_.reset();
}

bool PrefetchURLLoader::IsSignedExchangeHandlingEnabled() {
  if (NavigationURLLoaderImpl::IsNavigationLoaderOnUIEnabled()) {
    return signed_exchange_utils::IsSignedExchangeHandlingEnabled(
        browser_context_);
  }
  return signed_exchange_utils::IsSignedExchangeHandlingEnabledOnIO(
      resource_context_);
}

}  // namespace content
