// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_handle_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/navigator_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request_body.h"

namespace content {

NavigationHandleImpl::NavigationHandleImpl(
    NavigationRequest* navigation_request)
    : navigation_request_(navigation_request) {
  const GURL& url = navigation_request_->common_params().url;
  TRACE_EVENT_ASYNC_BEGIN2("navigation", "NavigationHandle", this,
                           "frame_tree_node", GetFrameTreeNodeId(), "url",
                           url.possibly_invalid_spec());
  DCHECK(!navigation_request_->common_params().navigation_start.is_null());
  DCHECK(!IsRendererDebugURL(url));

  if (IsInMainFrame()) {
    TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP1(
        "navigation", "Navigation StartToCommit", this,
        navigation_request_->common_params().navigation_start, "Initial URL",
        url.spec());
  }

  if (IsSameDocument()) {
    TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationHandle", this,
                                 "Same document");
  }
}

NavigationHandleImpl::~NavigationHandleImpl() {
  if (IsInMainFrame()) {
    TRACE_EVENT_ASYNC_END2("navigation", "Navigation StartToCommit", this,
                           "URL",
                           navigation_request_->common_params().url.spec(),
                           "Net Error Code", GetNetErrorCode());
  }
  TRACE_EVENT_ASYNC_END0("navigation", "NavigationHandle", this);
}

NavigatorDelegate* NavigationHandleImpl::GetDelegate() const {
  return frame_tree_node()->navigator()->GetDelegate();
}

int64_t NavigationHandleImpl::GetNavigationId() {
  return navigation_request_->navigation_handle_id();
}

const GURL& NavigationHandleImpl::GetURL() {
  return navigation_request_->common_params().url;
}

SiteInstanceImpl* NavigationHandleImpl::GetStartingSiteInstance() {
  return navigation_request_->starting_site_instance();
}

bool NavigationHandleImpl::IsInMainFrame() {
  return navigation_request_->IsInMainFrame();
}

bool NavigationHandleImpl::IsParentMainFrame() {
  return navigation_request_->IsParentMainFrame();
}

bool NavigationHandleImpl::IsRendererInitiated() {
  return !navigation_request_->browser_initiated();
}

bool NavigationHandleImpl::WasServerRedirect() {
  return navigation_request_->was_redirected();
}

const std::vector<GURL>& NavigationHandleImpl::GetRedirectChain() {
  return navigation_request_->redirect_chain();
}

int NavigationHandleImpl::GetFrameTreeNodeId() {
  return navigation_request_->GetFrameTreeNodeId();
}

RenderFrameHostImpl* NavigationHandleImpl::GetParentFrame() {
  return navigation_request_->GetParentFrame();
}

base::TimeTicks NavigationHandleImpl::NavigationStart() {
  return navigation_request_->common_params().navigation_start;
}

base::TimeTicks NavigationHandleImpl::NavigationInputStart() {
  return navigation_request_->common_params().input_start;
}

bool NavigationHandleImpl::IsPost() {
  return navigation_request_->common_params().method == "POST";
}

const scoped_refptr<network::ResourceRequestBody>&
NavigationHandleImpl::GetResourceRequestBody() {
  return navigation_request_->common_params().post_data;
}

const blink::mojom::Referrer& NavigationHandleImpl::GetReferrer() {
  return navigation_request_->sanitized_referrer();
}

bool NavigationHandleImpl::HasUserGesture() {
  return navigation_request_->common_params().has_user_gesture;
}

ui::PageTransition NavigationHandleImpl::GetPageTransition() {
  return navigation_request_->common_params().transition;
}

NavigationUIData* NavigationHandleImpl::GetNavigationUIData() {
  return navigation_request_->navigation_ui_data();
}

bool NavigationHandleImpl::IsExternalProtocol() {
  return navigation_request_->IsExternalProtocol();
}

net::Error NavigationHandleImpl::GetNetErrorCode() {
  return navigation_request_->net_error();
}

RenderFrameHostImpl* NavigationHandleImpl::GetRenderFrameHost() {
  return navigation_request_->render_frame_host();
}

bool NavigationHandleImpl::IsSameDocument() {
  return navigation_request_->IsSameDocument();
}

const net::HttpRequestHeaders& NavigationHandleImpl::GetRequestHeaders() {
  return navigation_request_->request_headers();
}

void NavigationHandleImpl::RemoveRequestHeader(const std::string& header_name) {
  navigation_request_->RemoveRequestHeader(header_name);
}

void NavigationHandleImpl::SetRequestHeader(const std::string& header_name,
                                            const std::string& header_value) {
  navigation_request_->SetRequestHeader(header_name, header_value);
}

const net::HttpResponseHeaders* NavigationHandleImpl::GetResponseHeaders() {
  return navigation_request_->GetResponseHeaders();
}

net::HttpResponseInfo::ConnectionInfo
NavigationHandleImpl::GetConnectionInfo() {
  return navigation_request_->GetConnectionInfo();
}

const base::Optional<net::SSLInfo>& NavigationHandleImpl::GetSSLInfo() {
  return navigation_request_->ssl_info();
}

const base::Optional<net::AuthChallengeInfo>&
NavigationHandleImpl::GetAuthChallengeInfo() {
  return navigation_request_->auth_challenge_info();
}

bool NavigationHandleImpl::HasCommitted() {
  return navigation_request_->HasCommitted();
}

bool NavigationHandleImpl::IsErrorPage() {
  return navigation_request_->IsErrorPage();
}

bool NavigationHandleImpl::HasSubframeNavigationEntryCommitted() {
  return navigation_request_->subframe_entry_committed();
}

bool NavigationHandleImpl::DidReplaceEntry() {
  return navigation_request_->did_replace_entry();
}

bool NavigationHandleImpl::ShouldUpdateHistory() {
  return navigation_request_->should_update_history();
}

const GURL& NavigationHandleImpl::GetPreviousURL() {
  return navigation_request_->previous_url();
}

net::IPEndPoint NavigationHandleImpl::GetSocketAddress() {
  return navigation_request_->GetSocketAddress();
}

void NavigationHandleImpl::RegisterThrottleForTesting(
    std::unique_ptr<NavigationThrottle> navigation_throttle) {
  navigation_request_->RegisterThrottleForTesting(
      std::move(navigation_throttle));
}

bool NavigationHandleImpl::IsDeferredForTesting() {
  return navigation_request_->IsDeferredForTesting();
}

bool NavigationHandleImpl::WasStartedFromContextMenu() {
  return navigation_request_->common_params().started_from_context_menu;
}

const GURL& NavigationHandleImpl::GetSearchableFormURL() {
  return navigation_request_->begin_params()->searchable_form_url;
}

const std::string& NavigationHandleImpl::GetSearchableFormEncoding() {
  return navigation_request_->begin_params()->searchable_form_encoding;
}

ReloadType NavigationHandleImpl::GetReloadType() {
  return navigation_request_->reload_type();
}

RestoreType NavigationHandleImpl::GetRestoreType() {
  return navigation_request_->restore_type();
}

const GURL& NavigationHandleImpl::GetBaseURLForDataURL() {
  return navigation_request_->common_params().base_url_for_data_url;
}

void NavigationHandleImpl::RegisterSubresourceOverride(
    mojom::TransferrableURLLoaderPtr transferrable_loader) {
  navigation_request_->RegisterSubresourceOverride(
      std::move(transferrable_loader));
}

const GlobalRequestID& NavigationHandleImpl::GetGlobalRequestID() {
  return navigation_request_->request_id();
}

bool NavigationHandleImpl::IsDownload() {
  return navigation_request_->is_download();
}

bool NavigationHandleImpl::IsFormSubmission() {
  return navigation_request_->begin_params()->is_form_submission;
}

bool NavigationHandleImpl::WasInitiatedByLinkClick() {
  return navigation_request_->begin_params()->was_initiated_by_link_click;
}

const std::string& NavigationHandleImpl::GetHrefTranslate() {
  return navigation_request_->common_params().href_translate;
}

void NavigationHandleImpl::CallResumeForTesting() {
  navigation_request_->CallResumeForTesting();
}

const base::Optional<url::Origin>& NavigationHandleImpl::GetInitiatorOrigin() {
  return navigation_request_->common_params().initiator_origin;
}

bool NavigationHandleImpl::IsSameProcess() {
  return navigation_request_->is_same_process();
}

int NavigationHandleImpl::GetNavigationEntryOffset() {
  return navigation_request_->navigation_entry_offset();
}

bool NavigationHandleImpl::FromDownloadCrossOriginRedirect() {
  return navigation_request_->from_download_cross_origin_redirect();
}

bool NavigationHandleImpl::IsSignedExchangeInnerResponse() {
  return navigation_request_->IsSignedExchangeInnerResponse();
}

bool NavigationHandleImpl::HasPrefetchedAlternativeSubresourceSignedExchange() {
  return navigation_request_
      ->HasPrefetchedAlternativeSubresourceSignedExchange();
}

bool NavigationHandleImpl::WasResponseCached() {
  return navigation_request_->WasResponseCached();
}

const net::ProxyServer& NavigationHandleImpl::GetProxyServer() {
  return navigation_request_->proxy_server();
}

}  // namespace content
