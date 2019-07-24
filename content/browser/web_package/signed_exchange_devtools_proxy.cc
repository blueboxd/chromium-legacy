// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_devtools_proxy.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/web_package/signed_exchange_envelope.h"
#include "content/browser/web_package/signed_exchange_error.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace content {

namespace {

// If on the UI thread, just run |task|, otherwise post a task to run
// the thread on the UI thread.
void RunOrPostTaskIfNotOnUiThread(const base::Location& from_here,
                                  base::OnceClosure task) {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    std::move(task).Run();
    return;
  }

  base::PostTaskWithTraits(from_here, {BrowserThread::UI}, std::move(task));
}

void AddErrorMessageToConsoleOnUI(
    base::RepeatingCallback<int(void)> frame_tree_node_id_getter,
    std::string error_message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  WebContents* web_contents =
      WebContents::FromFrameTreeNodeId(frame_tree_node_id_getter.Run());
  if (!web_contents)
    return;
  web_contents->GetMainFrame()->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kError, error_message);
}

void CertificateRequestSentOnUI(
    base::RepeatingCallback<int(void)> frame_tree_node_id_getter,
    const base::UnguessableToken& request_id,
    const base::UnguessableToken& loader_id,
    const network::ResourceRequest& request,
    const GURL& signed_exchange_url) {
  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id_getter.Run());
  if (!frame_tree_node)
    return;
  devtools_instrumentation::OnSignedExchangeCertificateRequestSent(
      frame_tree_node, request_id, loader_id, request, signed_exchange_url);
}

void CertificateResponseReceivedOnUI(
    base::RepeatingCallback<int(void)> frame_tree_node_id_getter,
    const base::UnguessableToken& request_id,
    const base::UnguessableToken& loader_id,
    const GURL& url,
    scoped_refptr<network::ResourceResponse> response) {
  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id_getter.Run());
  if (!frame_tree_node)
    return;
  devtools_instrumentation::OnSignedExchangeCertificateResponseReceived(
      frame_tree_node, request_id, loader_id, url, response->head);
}

void CertificateRequestCompletedOnUI(
    base::RepeatingCallback<int(void)> frame_tree_node_id_getter,
    const base::UnguessableToken& request_id,
    const network::URLLoaderCompletionStatus& status) {
  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id_getter.Run());
  if (!frame_tree_node)
    return;
  devtools_instrumentation::OnSignedExchangeCertificateRequestCompleted(
      frame_tree_node, request_id, status);
}

void OnSignedExchangeReceivedOnUI(
    base::RepeatingCallback<int(void)> frame_tree_node_id_getter,
    const GURL& outer_request_url,
    scoped_refptr<network::ResourceResponse> outer_response,
    base::Optional<const base::UnguessableToken> devtools_navigation_token,
    base::Optional<SignedExchangeEnvelope> envelope,
    scoped_refptr<net::X509Certificate> certificate,
    base::Optional<net::SSLInfo> ssl_info,
    std::vector<SignedExchangeError> errors) {
  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id_getter.Run());
  if (!frame_tree_node)
    return;
  devtools_instrumentation::OnSignedExchangeReceived(
      frame_tree_node, devtools_navigation_token, outer_request_url,
      outer_response->head, envelope, certificate, ssl_info, errors);
}

}  // namespace

SignedExchangeDevToolsProxy::SignedExchangeDevToolsProxy(
    const GURL& outer_request_url,
    const network::ResourceResponseHead& outer_response,
    base::RepeatingCallback<int(void)> frame_tree_node_id_getter,
    base::Optional<const base::UnguessableToken> devtools_navigation_token,
    bool report_raw_headers)
    : outer_request_url_(outer_request_url),
      outer_response_(outer_response),
      frame_tree_node_id_getter_(frame_tree_node_id_getter),
      devtools_navigation_token_(devtools_navigation_token),
      devtools_enabled_(report_raw_headers) {
  DCHECK_CURRENTLY_ON(
      NavigationURLLoaderImpl::GetLoaderRequestControllerThreadID());
}

SignedExchangeDevToolsProxy::~SignedExchangeDevToolsProxy() {
  DCHECK_CURRENTLY_ON(
      NavigationURLLoaderImpl::GetLoaderRequestControllerThreadID());
}

void SignedExchangeDevToolsProxy::ReportError(
    const std::string& message,
    base::Optional<SignedExchangeError::FieldIndexPair> error_field) {
  DCHECK_CURRENTLY_ON(
      NavigationURLLoaderImpl::GetLoaderRequestControllerThreadID());
  errors_.push_back(SignedExchangeError(message, std::move(error_field)));
  RunOrPostTaskIfNotOnUiThread(
      FROM_HERE,
      base::BindOnce(&AddErrorMessageToConsoleOnUI, frame_tree_node_id_getter_,
                     std::move(message)));
}

void SignedExchangeDevToolsProxy::CertificateRequestSent(
    const base::UnguessableToken& request_id,
    const network::ResourceRequest& request) {
  if (!devtools_enabled_)
    return;

  RunOrPostTaskIfNotOnUiThread(
      FROM_HERE,
      base::BindOnce(
          &CertificateRequestSentOnUI, frame_tree_node_id_getter_, request_id,
          devtools_navigation_token_ ? *devtools_navigation_token_ : request_id,
          request, outer_request_url_));
}

void SignedExchangeDevToolsProxy::CertificateResponseReceived(
    const base::UnguessableToken& request_id,
    const GURL& url,
    const network::ResourceResponseHead& head) {
  if (!devtools_enabled_)
    return;

  // Make a deep copy of ResourceResponseHead before passing it cross-thread.
  auto resource_response = base::MakeRefCounted<network::ResourceResponse>();
  resource_response->head = head;

  RunOrPostTaskIfNotOnUiThread(
      FROM_HERE,
      base::BindOnce(
          &CertificateResponseReceivedOnUI, frame_tree_node_id_getter_,
          request_id,
          devtools_navigation_token_ ? *devtools_navigation_token_ : request_id,
          url, resource_response->DeepCopy()));
}

void SignedExchangeDevToolsProxy::CertificateRequestCompleted(
    const base::UnguessableToken& request_id,
    const network::URLLoaderCompletionStatus& status) {
  if (!devtools_enabled_)
    return;
  RunOrPostTaskIfNotOnUiThread(
      FROM_HERE,
      base::BindOnce(&CertificateRequestCompletedOnUI,
                     frame_tree_node_id_getter_, request_id, status));
}

void SignedExchangeDevToolsProxy::OnSignedExchangeReceived(
    const base::Optional<SignedExchangeEnvelope>& envelope,
    const scoped_refptr<net::X509Certificate>& certificate,
    const net::SSLInfo* ssl_info) {
  DCHECK_CURRENTLY_ON(
      NavigationURLLoaderImpl::GetLoaderRequestControllerThreadID());
  if (!devtools_enabled_)
    return;
  base::Optional<net::SSLInfo> ssl_info_opt;
  if (ssl_info)
    ssl_info_opt = *ssl_info;

  // Make a deep copy of ResourceResponseHead before passing it cross-thread.
  auto resource_response = base::MakeRefCounted<network::ResourceResponse>();
  resource_response->head = outer_response_;

  RunOrPostTaskIfNotOnUiThread(
      FROM_HERE,
      base::BindOnce(&OnSignedExchangeReceivedOnUI, frame_tree_node_id_getter_,
                     outer_request_url_, resource_response->DeepCopy(),
                     devtools_navigation_token_, envelope, certificate,
                     std::move(ssl_info_opt), std::move(errors_)));
}

}  // namespace content
