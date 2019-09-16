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
    : navigation_request_(navigation_request) {}

NavigationHandleImpl::~NavigationHandleImpl() = default;

int64_t NavigationHandleImpl::GetNavigationId() {
  return navigation_request_->GetNavigationId();
}

const GURL& NavigationHandleImpl::GetURL() {
  return navigation_request_->GetURL();
}

SiteInstanceImpl* NavigationHandleImpl::GetStartingSiteInstance() {
  return navigation_request_->GetStartingSiteInstance();
}

bool NavigationHandleImpl::IsInMainFrame() {
  return navigation_request_->IsInMainFrame();
}

bool NavigationHandleImpl::IsParentMainFrame() {
  return navigation_request_->IsParentMainFrame();
}

bool NavigationHandleImpl::IsRendererInitiated() {
  return navigation_request_->IsRendererInitiated();
}

bool NavigationHandleImpl::WasServerRedirect() {
  return navigation_request_->WasServerRedirect();
}

const std::vector<GURL>& NavigationHandleImpl::GetRedirectChain() {
  return navigation_request_->GetRedirectChain();
}

int NavigationHandleImpl::GetFrameTreeNodeId() {
  return navigation_request_->GetFrameTreeNodeId();
}

RenderFrameHostImpl* NavigationHandleImpl::GetParentFrame() {
  return navigation_request_->GetParentFrame();
}

base::TimeTicks NavigationHandleImpl::NavigationStart() {
  return navigation_request_->NavigationStart();
}

base::TimeTicks NavigationHandleImpl::NavigationInputStart() {
  return navigation_request_->NavigationInputStart();
}

bool NavigationHandleImpl::IsPost() {
  return navigation_request_->IsPost();
}

const blink::mojom::Referrer& NavigationHandleImpl::GetReferrer() {
  return navigation_request_->GetReferrer();
}

bool NavigationHandleImpl::HasUserGesture() {
  return navigation_request_->HasUserGesture();
}

ui::PageTransition NavigationHandleImpl::GetPageTransition() {
  return navigation_request_->GetPageTransition();
}

NavigationUIData* NavigationHandleImpl::GetNavigationUIData() {
  return navigation_request_->GetNavigationUIData();
}

bool NavigationHandleImpl::IsExternalProtocol() {
  return navigation_request_->IsExternalProtocol();
}

net::Error NavigationHandleImpl::GetNetErrorCode() {
  return navigation_request_->GetNetErrorCode();
}

RenderFrameHostImpl* NavigationHandleImpl::GetRenderFrameHost() {
  return navigation_request_->GetRenderFrameHost();
}

bool NavigationHandleImpl::IsSameDocument() {
  return navigation_request_->IsSameDocument();
}

const net::HttpRequestHeaders& NavigationHandleImpl::GetRequestHeaders() {
  return navigation_request_->GetRequestHeaders();
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
  return navigation_request_->GetSSLInfo();
}

const base::Optional<net::AuthChallengeInfo>&
NavigationHandleImpl::GetAuthChallengeInfo() {
  return navigation_request_->GetAuthChallengeInfo();
}

bool NavigationHandleImpl::HasCommitted() {
  return navigation_request_->HasCommitted();
}

bool NavigationHandleImpl::IsErrorPage() {
  return navigation_request_->IsErrorPage();
}

bool NavigationHandleImpl::HasSubframeNavigationEntryCommitted() {
  return navigation_request_->HasSubframeNavigationEntryCommitted();
}

bool NavigationHandleImpl::DidReplaceEntry() {
  return navigation_request_->DidReplaceEntry();
}

bool NavigationHandleImpl::ShouldUpdateHistory() {
  return navigation_request_->ShouldUpdateHistory();
}

const GURL& NavigationHandleImpl::GetPreviousURL() {
  return navigation_request_->GetPreviousURL();
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
  return navigation_request_->WasStartedFromContextMenu();
}

const GURL& NavigationHandleImpl::GetSearchableFormURL() {
  return navigation_request_->GetSearchableFormURL();
}

const std::string& NavigationHandleImpl::GetSearchableFormEncoding() {
  return navigation_request_->GetSearchableFormEncoding();
}

ReloadType NavigationHandleImpl::GetReloadType() {
  return navigation_request_->GetReloadType();
}

RestoreType NavigationHandleImpl::GetRestoreType() {
  return navigation_request_->GetRestoreType();
}

const GURL& NavigationHandleImpl::GetBaseURLForDataURL() {
  return navigation_request_->GetBaseURLForDataURL();
}

void NavigationHandleImpl::RegisterSubresourceOverride(
    mojom::TransferrableURLLoaderPtr transferrable_loader) {
  navigation_request_->RegisterSubresourceOverride(
      std::move(transferrable_loader));
}

const GlobalRequestID& NavigationHandleImpl::GetGlobalRequestID() {
  return navigation_request_->GetGlobalRequestID();
}

bool NavigationHandleImpl::IsDownload() {
  return navigation_request_->IsDownload();
}

bool NavigationHandleImpl::IsFormSubmission() {
  return navigation_request_->IsFormSubmission();
}

bool NavigationHandleImpl::WasInitiatedByLinkClick() {
  return navigation_request_->WasInitiatedByLinkClick();
}

const std::string& NavigationHandleImpl::GetHrefTranslate() {
  return navigation_request_->GetHrefTranslate();
}

void NavigationHandleImpl::CallResumeForTesting() {
  navigation_request_->CallResumeForTesting();
}

const base::Optional<url::Origin>& NavigationHandleImpl::GetInitiatorOrigin() {
  return navigation_request_->GetInitiatorOrigin();
}

bool NavigationHandleImpl::IsSameProcess() {
  return navigation_request_->IsSameProcess();
}

int NavigationHandleImpl::GetNavigationEntryOffset() {
  return navigation_request_->GetNavigationEntryOffset();
}

bool NavigationHandleImpl::FromDownloadCrossOriginRedirect() {
  return navigation_request_->FromDownloadCrossOriginRedirect();
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
  return navigation_request_->GetProxyServer();
}

GlobalFrameRoutingId NavigationHandleImpl::GetPreviousRenderFrameHostId() {
  return navigation_request_->GetPreviousRenderFrameHostId();
}

}  // namespace content
