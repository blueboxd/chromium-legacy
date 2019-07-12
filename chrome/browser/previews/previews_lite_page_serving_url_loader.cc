// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_serving_url_loader.h"

#include <stdint.h>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/previews/previews_lite_page_decider.h"
#include "chrome/browser/previews/previews_lite_page_navigation_throttle.h"
#include "chrome/browser/previews/previews_lite_page_navigation_throttle_manager.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_lite_page_redirect.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/previews_state.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace previews {

namespace {

void BlacklistBypassedHostOnUIThread(const std::string& host,
                                     base::TimeDelta duration,
                                     int frame_tree_node_id) {
  auto* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  // If the WebContents has been closed this may be null.
  if (!web_contents)
    return;

  PreviewsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()))
      ->previews_lite_page_decider()
      ->BlacklistBypassedHost(host, duration);
}

void BlacklistBypassedHost(const std::string& host,
                           base::TimeDelta duration,
                           int frame_tree_node_id) {
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&BlacklistBypassedHostOnUIThread, host, duration,
                     frame_tree_node_id));
}

void SetServerUnavailableForOnUIThread(base::TimeDelta duration,
                                       int frame_tree_node_id) {
  auto* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  // If the WebContents has been closed this may be null.
  if (!web_contents)
    return;

  PreviewsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()))
      ->previews_lite_page_decider()
      ->SetServerUnavailableFor(duration);
}

void SetServerUnavailableFor(base::TimeDelta duration, int frame_tree_node_id) {
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&SetServerUnavailableForOnUIThread, duration,
                     frame_tree_node_id));
}

void ReportDataSavingsOnUIThread(int64_t network_bytes,
                                 int64_t original_bytes,
                                 std::string host,
                                 int frame_tree_node_id) {
  auto* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  // If the WebContents has been closed this may be null.
  if (!web_contents)
    return;

  PreviewsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()))
      ->previews_lite_page_decider()
      ->ReportDataSavings(network_bytes, original_bytes, host);
}

void ReportDataSavings(int64_t network_bytes,
                       int64_t original_bytes,
                       std::string host,
                       int frame_tree_node_id) {
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReportDataSavingsOnUIThread, network_bytes,
                     original_bytes, std::move(host), frame_tree_node_id));
}

const base::TimeDelta kBlacklistDuration = base::TimeDelta::FromDays(30);

// Used for mojo pipe size. Same constant as navigation code.
constexpr size_t kServingDefaultAllocationSize = 512 * 1024;

const net::NetworkTrafficAnnotationTag kPreviewsTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("https_server_previews_navigation", R"(
      semantics {
        sender: "HTTPS server previews navigation URL Loader"
        description:
          "This request is issued by a main frame navigation to fetch a server "
          "generated lite page version of page."
        trigger:
          "Navigating Chrome (by clicking on a link, bookmark, history item, "
          "using session restore, etc) on a slow page."
        data:
          "Arbitrary site-controlled data can be included in the URL, HTTP "
          "headers, and request body."
        destination: WEBSITE
      }
      policy {
        cookies_allowed: YES
        cookies_store: "user"
        setting: "Disable Data Saver on Android (Settings > Data Saver)."
        chrome_policy {
          URLBlacklist {
            URLBlacklist: { entries: '*' }
          }
        }
        chrome_policy {
          URLWhitelist {
            URLWhitelist { }
          }
        }
      }
      )");

}  // namespace

PreviewsLitePageServingURLLoader::PreviewsLitePageServingURLLoader(
    ResultCallback result_callback)
    : url_loader_binding_(this),
      result_callback_(std::move(result_callback)),
      binding_(this) {}

void PreviewsLitePageServingURLLoader::StartNetworkRequest(
    const network::ResourceRequest& request,
    const scoped_refptr<network::SharedURLLoaderFactory>&
        network_loader_factory,
    int frame_tree_node_id) {
  frame_tree_node_id_ = frame_tree_node_id;
  previews_url_ = request.url;
  network::mojom::URLLoaderClientPtr client;

  url_loader_binding_.Bind(mojo::MakeRequest(&client),
                           base::ThreadTaskRunnerHandle::Get());
  url_loader_binding_.set_connection_error_handler(
      base::BindOnce(&PreviewsLitePageServingURLLoader::OnConnectionError,
                     base::Unretained(this)));

  // Create a network service URL loader with passed in params.
  network_loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&network_url_loader_), frame_tree_node_id_, 0,
      network::mojom::kURLLoadOptionNone, request, std::move(client),
      net::MutableNetworkTrafficAnnotationTag(kPreviewsTrafficAnnotation));

  timeout_timer_.Start(
      FROM_HERE, previews::params::LitePagePreviewsNavigationTimeoutDuration(),
      base::BindOnce(&PreviewsLitePageServingURLLoader::Timeout,
                     weak_ptr_factory_.GetWeakPtr()));
}

PreviewsLitePageServingURLLoader::~PreviewsLitePageServingURLLoader() = default;

void PreviewsLitePageServingURLLoader::Timeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the result is already determined, don't do anything.
  if (result_callback_.is_null())
    return;

  UMA_HISTOGRAM_ENUMERATION(
      "Previews.ServerLitePage.ServerResponse",
      PreviewsLitePageNavigationThrottle::ServerResponse::kTimeout);

  Fallback();
}

void PreviewsLitePageServingURLLoader::Fallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(result_callback_)
      .Run(ServingLoaderResult::kFallback, base::nullopt, nullptr);
}

RequestHandler PreviewsLitePageServingURLLoader::ServingResponseHandler() {
  DCHECK(result_callback_.is_null());
  return base::BindOnce(
      &PreviewsLitePageServingURLLoader::SetUpForwardingClient,
      weak_ptr_factory_.GetWeakPtr());
}

void PreviewsLitePageServingURLLoader::SetUpForwardingClient(
    const network::ResourceRequest& /* resource_request */,
    network::mojom::URLLoaderRequest request,
    network::mojom::URLLoaderClientPtr forwarding_client) {
  // Bind to the content/ navigation code.
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler(
      base::BindOnce(&PreviewsLitePageServingURLLoader::OnConnectionError,
                     weak_ptr_factory_.GetWeakPtr()));
  forwarding_client_ = std::move(forwarding_client);

  // If there was an URLLoader error between handing off this handler and
  // running it, don't handle the request.
  if (!network_url_loader_) {
    binding_.Close();
    forwarding_client_.reset();
    delete this;
    return;
  }

  mojo::DataPipe pipe(kServingDefaultAllocationSize);
  if (!pipe.consumer_handle.is_valid()) {
    network_url_loader_.reset();
    url_loader_binding_.Close();
    delete this;
    return;
  }

  forwarding_client_->OnReceiveResponse(resource_response_->head);

  // Resume previously paused network service URLLoader.
  url_loader_binding_.ResumeIncomingMethodCallProcessing();
}

void PreviewsLitePageServingURLLoader::OnReceiveResponse(
    const network::ResourceResponseHead& head) {
  DCHECK(!forwarding_client_);

  const net::HttpResponseHeaders* response_headers = head.headers.get();
  // TODO: evaluate all the responses we allow, don't hard code 200.
  if (!response_headers) {
    UMA_HISTOGRAM_ENUMERATION(
        "Previews.ServerLitePage.ServerResponse",
        PreviewsLitePageNavigationThrottle::ServerResponse::kFailed);
    Fallback();
    return;
  }

  if (response_headers->response_code() != net::HTTP_OK) {
    if (response_headers->response_code() == net::HTTP_SERVICE_UNAVAILABLE) {
      UMA_HISTOGRAM_ENUMERATION("Previews.ServerLitePage.ServerResponse",
                                PreviewsLitePageNavigationThrottle::
                                    ServerResponse::kServiceUnavailable);
      std::string retry_after_header;
      base::TimeDelta retry_after = base::TimeDelta::FromSeconds(base::RandInt(
          60, previews::params::PreviewServerLoadshedMaxSeconds()));
      if (response_headers->EnumerateHeader(nullptr, "retry-after",
                                            &retry_after_header)) {
        net::HttpUtil::ParseRetryAfterHeader(retry_after_header,
                                             base::Time::Now(), &retry_after);
      }
      SetServerUnavailableFor(retry_after, frame_tree_node_id_);
    } else if (response_headers->response_code() == net::HTTP_FORBIDDEN) {
      UMA_HISTOGRAM_ENUMERATION(
          "Previews.ServerLitePage.ServerResponse",
          PreviewsLitePageNavigationThrottle::ServerResponse::kAuthFailure);
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "Previews.ServerLitePage.ServerResponse",
          PreviewsLitePageNavigationThrottle::ServerResponse::kOther);
    }

    Fallback();
    return;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "Previews.ServerLitePage.ServerResponse",
      PreviewsLitePageNavigationThrottle::ServerResponse::kOk);

  // Store head and pause new messages until the forwarding client is set up.
  // Make a deep copy of ResourceResponseHead before passing it cross-thread.
  resource_response_ = base::MakeRefCounted<network::ResourceResponse>();
  resource_response_->head = head;

  const int64_t ofcl =
      data_reduction_proxy::GetDataReductionProxyOFCL(response_headers);
  if (ofcl > 0) {
    std::string original_url;
    previews::ExtractOriginalURLFromLitePageRedirectURL(previews_url_,
                                                        &original_url);

    ReportDataSavings(response_headers->GetContentLength(), ofcl,
                      GURL(original_url).host(), frame_tree_node_id_);
  }

  url_loader_binding_.PauseIncomingMethodCallProcessing();
  std::move(result_callback_)
      .Run(ServingLoaderResult::kSuccess, base::nullopt, nullptr);
}

void PreviewsLitePageServingURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& head) {
  DCHECK(!forwarding_client_);

  // Store head and pause new messages until the forwarding client is set up.
  // Make a deep copy of ResourceResponseHead before passing it cross-thread.
  resource_response_ = base::MakeRefCounted<network::ResourceResponse>();
  resource_response_->head = head;

  // If the URL we are redirecting to is the one we started at, we should
  // fallback after checking headers for bypass instructions.
  std::string original_url;
  if (previews::ExtractOriginalURLFromLitePageRedirectURL(previews_url_,
                                                          &original_url) &&
      GURL(original_url) == redirect_info.new_url) {
    UMA_HISTOGRAM_ENUMERATION("Previews.ServerLitePage.ServerResponse",
                              PreviewsLitePageNavigationThrottle::
                                  ServerResponse::kPreviewUnavailable);
    const net::HttpResponseHeaders* response_headers = head.headers.get();

    std::string chrome_proxy_header;
    bool blacklist_host =
        response_headers &&
        response_headers->EnumerateHeader(
            nullptr, data_reduction_proxy::chrome_proxy_header(),
            &chrome_proxy_header) &&
        chrome_proxy_header.find("host-blacklisted") != std::string::npos;

    if (blacklist_host)
      BlacklistBypassedHost(GURL(original_url).host(), kBlacklistDuration,
                            frame_tree_node_id_);

    UMA_HISTOGRAM_BOOLEAN("Previews.ServerLitePage.HostBlacklistedOnBypass",
                          blacklist_host);

    Fallback();
    return;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "Previews.ServerLitePage.ServerResponse",
      PreviewsLitePageNavigationThrottle::ServerResponse::kRedirect);

  std::move(result_callback_)
      .Run(ServingLoaderResult::kRedirect, redirect_info, resource_response_);
}

void PreviewsLitePageServingURLLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback callback) {
  // We only handle GETs.
  NOTREACHED();
}

void PreviewsLitePageServingURLLoader::OnReceiveCachedMetadata(
    mojo_base::BigBuffer data) {
  // Do nothing. This is not supported for navigation loader.
}

void PreviewsLitePageServingURLLoader::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  DCHECK(forwarding_client_);
  forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void PreviewsLitePageServingURLLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK(forwarding_client_);
  forwarding_client_->OnStartLoadingResponseBody(std::move(body));
}

void PreviewsLitePageServingURLLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  if (forwarding_client_) {
    forwarding_client_->OnComplete(status);
    return;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "Previews.ServerLitePage.ServerResponse",
      PreviewsLitePageNavigationThrottle::ServerResponse::kFailed);

  // If OnComplete is called before, OnReceiveResponse, this is indicative of a
  // failure of some sort.
  Fallback();
}

void PreviewsLitePageServingURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const base::Optional<GURL>& new_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This should never be called for a non-network service URLLoader.
  NOTREACHED();
}

void PreviewsLitePageServingURLLoader::ProceedWithResponse() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Pass through.
  network_url_loader_->ProceedWithResponse();
}

void PreviewsLitePageServingURLLoader::SetPriority(
    net::RequestPriority priority,
    int32_t intra_priority_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Pass through.
  network_url_loader_->SetPriority(priority, intra_priority_value);
}

void PreviewsLitePageServingURLLoader::PauseReadingBodyFromNet() {
  // Pass through.
  network_url_loader_->PauseReadingBodyFromNet();
}

void PreviewsLitePageServingURLLoader::ResumeReadingBodyFromNet() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Pass through.
  network_url_loader_->ResumeReadingBodyFromNet();
}

void PreviewsLitePageServingURLLoader::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // When we are not yet bound to the navigation code, fallback, which will
  // destroy this object.
  if (!result_callback_.is_null()) {
    UMA_HISTOGRAM_ENUMERATION(
        "Previews.ServerLitePage.ServerResponse",
        PreviewsLitePageNavigationThrottle::ServerResponse::kFailed);
    Fallback();
    return;
  }

  network_url_loader_.reset();
  url_loader_binding_.Close();

  if (binding_.is_bound()) {
    binding_.Close();
    forwarding_client_.reset();
    delete this;
  }
}

}  // namespace previews
