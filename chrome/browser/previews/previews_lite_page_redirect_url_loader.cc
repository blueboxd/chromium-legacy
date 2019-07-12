// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_redirect_url_loader.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/previews/previews_lite_page_navigation_throttle.h"
#include "components/previews/core/previews_lite_page_redirect.h"
#include "content/public/common/previews_state.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_util.h"
#include "services/network/public/cpp/resource_request.h"

namespace previews {

namespace {
// Used for mojo pipe size. Same constant as navigation code.
constexpr size_t kRedirectDefaultAllocationSize = 512 * 1024;
}  // namespace

PreviewsLitePageRedirectURLLoader::PreviewsLitePageRedirectURLLoader(
    const network::ResourceRequest& tentative_resource_request,
    HandleRequest callback)
    : modified_resource_request_(tentative_resource_request),
      callback_(std::move(callback)),
      binding_(this) {}

PreviewsLitePageRedirectURLLoader::~PreviewsLitePageRedirectURLLoader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PreviewsLitePageRedirectURLLoader::StartRedirectToPreview(
    const net::HttpRequestHeaders& chrome_proxy_headers,
    const scoped_refptr<network::SharedURLLoaderFactory>&
        network_loader_factory,
    int frame_tree_node_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GURL lite_page_url = PreviewsLitePageNavigationThrottle::GetPreviewsURLForURL(
      modified_resource_request_.url);

  CreateRedirectInformation(lite_page_url);

  modified_resource_request_.headers.MergeFrom(chrome_proxy_headers);

  serving_url_loader_ = std::make_unique<PreviewsLitePageServingURLLoader>(
      base::BindOnce(&PreviewsLitePageRedirectURLLoader::OnResultDetermined,
                     weak_ptr_factory_.GetWeakPtr()));
  // |serving_url_loader_| can be null after this call.
  serving_url_loader_->StartNetworkRequest(
      modified_resource_request_, network_loader_factory, frame_tree_node_id);
}

void PreviewsLitePageRedirectURLLoader::StartRedirectToOriginalURL(
    const GURL& original_url) {
  CreateRedirectInformation(original_url);

  std::move(callback_).Run(
      nullptr, base::BindOnce(&PreviewsLitePageRedirectURLLoader::
                                  StartHandlingRedirectToModifiedRequest,
                              weak_ptr_factory_.GetWeakPtr()));
}

void PreviewsLitePageRedirectURLLoader::CreateRedirectInformation(
    const GURL& redirect_url) {
  bool insecure_scheme_was_upgraded = false;
  bool copy_fragment = true;

  redirect_info_ = net::RedirectInfo::ComputeRedirectInfo(
      modified_resource_request_.method, modified_resource_request_.url,
      modified_resource_request_.site_for_cookies,
      modified_resource_request_.top_frame_origin,
      net::URLRequest::UPDATE_FIRST_PARTY_URL_ON_REDIRECT,
      modified_resource_request_.referrer_policy,
      modified_resource_request_.referrer.spec(), net::HTTP_TEMPORARY_REDIRECT,
      redirect_url, base::nullopt, insecure_scheme_was_upgraded, copy_fragment);

  bool should_clear_upload = false;
  net::RedirectUtil::UpdateHttpRequest(
      modified_resource_request_.url, modified_resource_request_.method,
      redirect_info_, base::nullopt, base::nullopt,
      &modified_resource_request_.headers, &should_clear_upload);

  DCHECK(!should_clear_upload);

  modified_resource_request_.url = redirect_info_.new_url;
  modified_resource_request_.method = redirect_info_.new_method;
  modified_resource_request_.site_for_cookies =
      redirect_info_.new_site_for_cookies;
  modified_resource_request_.top_frame_origin =
      redirect_info_.new_top_frame_origin;
  modified_resource_request_.referrer = GURL(redirect_info_.new_referrer);
  modified_resource_request_.referrer_policy =
      redirect_info_.new_referrer_policy;
}

void PreviewsLitePageRedirectURLLoader::OnResultDetermined(
    ServingLoaderResult result,
    base::Optional<net::RedirectInfo> redirect_info,
    scoped_refptr<network::ResourceResponse> response) {
  DCHECK(!redirect_info || result == ServingLoaderResult::kRedirect);
  DCHECK(!response || result == ServingLoaderResult::kRedirect);
  switch (result) {
    case ServingLoaderResult::kSuccess:
      OnLitePageSuccess();
      return;
    case ServingLoaderResult::kFallback:
      OnLitePageFallback();
      return;
    case ServingLoaderResult::kRedirect:
      OnLitePageRedirect(redirect_info.value(), response->head);
      return;
  }
  NOTREACHED();
}

void PreviewsLitePageRedirectURLLoader::OnLitePageSuccess() {
  std::move(callback_).Run(
      std::move(serving_url_loader_),
      base::BindOnce(&PreviewsLitePageRedirectURLLoader::
                         StartHandlingRedirectToModifiedRequest,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PreviewsLitePageRedirectURLLoader::OnLitePageRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& response_head) {
  redirect_info_ = redirect_info;

  response_head_ = response_head;

  std::move(callback_).Run(
      nullptr,
      base::BindOnce(&PreviewsLitePageRedirectURLLoader::StartHandlingRedirect,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PreviewsLitePageRedirectURLLoader::OnLitePageFallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback_).Run(nullptr, {});
}

void PreviewsLitePageRedirectURLLoader::StartHandlingRedirectToModifiedRequest(
    const network::ResourceRequest& resource_request,
    network::mojom::URLLoaderRequest request,
    network::mojom::URLLoaderClientPtr client) {
  response_head_.request_start = base::TimeTicks::Now();
  response_head_.response_start = response_head_.request_start;

  std::string header_string = base::StringPrintf(
      "HTTP/1.1 %i Temporary Redirect\n"
      "Location: %s\n",
      net::HTTP_TEMPORARY_REDIRECT,
      modified_resource_request_.url.spec().c_str());

  response_head_.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(header_string));
  response_head_.encoded_data_length = 0;

  StartHandlingRedirect(resource_request, std::move(request),
                        std::move(client));
}

void PreviewsLitePageRedirectURLLoader::StartHandlingRedirect(
    const network::ResourceRequest& /* resource_request */,
    network::mojom::URLLoaderRequest request,
    network::mojom::URLLoaderClientPtr client) {
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler(
      base::BindOnce(&PreviewsLitePageRedirectURLLoader::OnConnectionClosed,
                     weak_ptr_factory_.GetWeakPtr()));
  client_ = std::move(client);

  mojo::DataPipe pipe(kRedirectDefaultAllocationSize);
  if (!pipe.consumer_handle.is_valid()) {
    delete this;
    return;
  }

  client_->OnReceiveRedirect(redirect_info_, response_head_);
}

void PreviewsLitePageRedirectURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const base::Optional<GURL>& new_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Content should not hang onto old URLLoaders for redirects.
  NOTREACHED();
}

void PreviewsLitePageRedirectURLLoader::ProceedWithResponse() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This class never provides a response past the headers.
  NOTREACHED();
}

void PreviewsLitePageRedirectURLLoader::SetPriority(
    net::RequestPriority priority,
    int32_t intra_priority_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Pass through.
  serving_url_loader_->SetPriority(priority, intra_priority_value);
}

void PreviewsLitePageRedirectURLLoader::PauseReadingBodyFromNet() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Pass through.
  serving_url_loader_->PauseReadingBodyFromNet();
}

void PreviewsLitePageRedirectURLLoader::ResumeReadingBodyFromNet() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Pass through.
  serving_url_loader_->ResumeReadingBodyFromNet();
}

void PreviewsLitePageRedirectURLLoader::OnConnectionClosed() {
  // This happens when content cancels the navigation. Close the network request
  // and client handle and destroy |this|.
  binding_.Close();
  client_.reset();
  delete this;
}

}  // namespace previews
