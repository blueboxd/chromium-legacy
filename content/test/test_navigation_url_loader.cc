// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_navigation_url_loader.h"

#include <utility>

#include "content/browser/loader/navigation_url_loader_delegate.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/navigation_policy.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

TestNavigationURLLoader::TestNavigationURLLoader(
    std::unique_ptr<NavigationRequestInfo> request_info,
    NavigationURLLoaderDelegate* delegate)
    : request_info_(std::move(request_info)),
      delegate_(delegate),
      redirect_count_(0),
      response_proceeded_(false) {
}

void TestNavigationURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    PreviewsState new_previews_state) {
  redirect_count_++;
}

void TestNavigationURLLoader::ProceedWithResponse() {
  response_proceeded_ = true;
}

void TestNavigationURLLoader::SimulateServerRedirect(const GURL& redirect_url) {
  net::RedirectInfo redirect_info;
  redirect_info.status_code = 302;
  redirect_info.new_method = "GET";
  redirect_info.new_url = redirect_url;
  redirect_info.new_site_for_cookies = redirect_url;
  scoped_refptr<network::ResourceResponse> response_head(
      new network::ResourceResponse);
  CallOnRequestRedirected(redirect_info, response_head);
}

void TestNavigationURLLoader::SimulateError(int error_code) {
  SimulateErrorWithStatus(network::URLLoaderCompletionStatus(error_code));
}

void TestNavigationURLLoader::SimulateErrorWithStatus(
    const network::URLLoaderCompletionStatus& status) {
  delegate_->OnRequestFailed(status);
}

void TestNavigationURLLoader::CallOnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    const scoped_refptr<network::ResourceResponse>& response_head) {
  delegate_->OnRequestRedirected(redirect_info, response_head);
}

void TestNavigationURLLoader::CallOnResponseStarted(
    const scoped_refptr<network::ResourceResponse>& response_head) {
  // Start the request_ids at 1000 to avoid collisions with request ids from
  // network resources (it should be rare to compare these in unit tests).
  static int request_id = 1000;
  int child_id =
      WebContents::FromFrameTreeNodeId(request_info_->frame_tree_node_id)
          ->GetMainFrame()
          ->GetProcess()
          ->GetID();
  GlobalRequestID global_id(child_id, ++request_id);

  // Create a bidirectionnal communication pipe between a URLLoader and a
  // URLLoaderClient. It will be closed at the end of this function. The sole
  // purpose of this is not to violate some DCHECKs when the navigation commits.
  network::mojom::URLLoaderClientPtr url_loader_client_ptr;
  network::mojom::URLLoaderClientRequest url_loader_client_request =
      mojo::MakeRequest(&url_loader_client_ptr);
  network::mojom::URLLoaderPtr url_loader_ptr;
  network::mojom::URLLoaderRequest url_loader_request =
      mojo::MakeRequest(&url_loader_ptr);
  auto url_loader_client_endpoints =
      network::mojom::URLLoaderClientEndpoints::New(
          url_loader_ptr.PassInterface(), std::move(url_loader_client_request));

  delegate_->OnResponseStarted(
      std::move(url_loader_client_endpoints), response_head,
      mojo::ScopedDataPipeConsumerHandle(), global_id, false,
      NavigationDownloadPolicy(), base::nullopt);
}

TestNavigationURLLoader::~TestNavigationURLLoader() {}

}  // namespace content
