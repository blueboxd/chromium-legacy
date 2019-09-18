// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATION_HANDLE_IMPL_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATION_HANDLE_IMPL_H_

#include "content/public/browser/navigation_handle.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigation_throttle_runner.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_type.h"
#include "content/public/browser/restore_type.h"
#include "net/base/ip_endpoint.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/platform/web_mixed_content_context_type.h"
#include "url/gurl.h"

namespace content {

struct GlobalFrameRoutingId;
class NavigationUIData;
class SiteInstanceImpl;

// This class keeps track of a single navigation. It is created after the
// BeforeUnload for the navigation has run. It is then owned by the
// NavigationRequest until the navigation is ready to commit. The
// NavigationHandleImpl ownership is then transferred to the RenderFrameHost in
// which the navigation will commit. It is finaly destroyed when the navigation
// commits.
class CONTENT_EXPORT NavigationHandleImpl : public NavigationHandle {
 public:
  ~NavigationHandleImpl() override;

  // NavigationHandle implementation:
  int64_t GetNavigationId() override;
  const GURL& GetURL() override;
  SiteInstanceImpl* GetStartingSiteInstance() override;
  bool IsInMainFrame() override;
  bool IsParentMainFrame() override;
  bool IsRendererInitiated() override;
  bool WasServerRedirect() override;
  const std::vector<GURL>& GetRedirectChain() override;
  int GetFrameTreeNodeId() override;
  RenderFrameHostImpl* GetParentFrame() override;
  base::TimeTicks NavigationStart() override;
  base::TimeTicks NavigationInputStart() override;
  bool IsPost() override;
  const blink::mojom::Referrer& GetReferrer() override;
  bool HasUserGesture() override;
  ui::PageTransition GetPageTransition() override;
  NavigationUIData* GetNavigationUIData() override;
  bool IsExternalProtocol() override;
  net::Error GetNetErrorCode() override;
  RenderFrameHostImpl* GetRenderFrameHost() override;
  GlobalFrameRoutingId GetPreviousRenderFrameHostId() override;
  bool IsSameDocument() override;
  bool HasCommitted() override;
  bool IsErrorPage() override;
  bool HasSubframeNavigationEntryCommitted() override;
  bool DidReplaceEntry() override;
  bool ShouldUpdateHistory() override;
  const GURL& GetPreviousURL() override;
  net::IPEndPoint GetSocketAddress() override;
  const net::HttpRequestHeaders& GetRequestHeaders() override;
  void RemoveRequestHeader(const std::string& header_name) override;
  void SetRequestHeader(const std::string& header_name,
                        const std::string& header_value) override;
  const net::HttpResponseHeaders* GetResponseHeaders() override;
  net::HttpResponseInfo::ConnectionInfo GetConnectionInfo() override;
  const base::Optional<net::SSLInfo>& GetSSLInfo() override;
  const base::Optional<net::AuthChallengeInfo>& GetAuthChallengeInfo() override;
  void RegisterThrottleForTesting(
      std::unique_ptr<NavigationThrottle> navigation_throttle) override;
  bool IsDeferredForTesting() override;
  bool WasStartedFromContextMenu() override;
  const GURL& GetSearchableFormURL() override;
  const std::string& GetSearchableFormEncoding() override;
  ReloadType GetReloadType() override;
  RestoreType GetRestoreType() override;
  const GURL& GetBaseURLForDataURL() override;
  const GlobalRequestID& GetGlobalRequestID() override;
  bool IsDownload() override;
  bool IsFormSubmission() override;
  bool WasInitiatedByLinkClick() override;
  bool IsSignedExchangeInnerResponse() override;
  bool HasPrefetchedAlternativeSubresourceSignedExchange() override;
  bool WasResponseCached() override;
  const net::ProxyServer& GetProxyServer() override;
  const std::string& GetHrefTranslate() override;
  const base::Optional<url::Origin>& GetInitiatorOrigin() override;
  bool IsSameProcess() override;
  int GetNavigationEntryOffset() override;
  bool FromDownloadCrossOriginRedirect() override;

  // Returns the NavigationRequest which owns this NavigationHandle.
  NavigationRequest* navigation_request() { return navigation_request_; }

  // Simulates the navigation resuming. Most callers should just let the
  // deferring NavigationThrottle do the resuming.
  void CallResumeForTesting();

  void RegisterSubresourceOverride(
      mojom::TransferrableURLLoaderPtr transferrable_loader) override;

  blink::mojom::RequestContextType request_context_type() const {
    return navigation_request_->request_context_type();
  }

  // Get the unique id from the NavigationEntry associated with this
  // NavigationHandle. Note that a synchronous, renderer-initiated navigation
  // will not have a NavigationEntry associated with it, and this will return 0.
  int pending_nav_entry_id() const {
    return navigation_request_->nav_entry_id();
  }

  // Returns the FrameTreeNode this navigation is happening in.
  FrameTreeNode* frame_tree_node() const {
    return navigation_request_->frame_tree_node();
  }

  NavigationType navigation_type() {
    return navigation_request_->navigation_type();
  }

  CSPDisposition should_check_main_world_csp() const {
    return navigation_request_->common_params()
        .initiator_csp_info.should_check_main_world_csp;
  }

  const base::Optional<SourceLocation>& source_location() const {
    return navigation_request_->common_params().source_location;
  }

 private:
  friend class NavigationRequest;

  // If |redirect_chain| is empty, then the redirect chain will be created to
  // start with |url|. Otherwise |redirect_chain| is used as the starting point.
  // |navigation_start| comes from the CommonNavigationParams associated with
  // this navigation.
  NavigationHandleImpl(NavigationRequest* navigation_request);

  NavigationRequest::NavigationHandleState state() const {
    return navigation_request_->handle_state();
  }

  // The NavigationRequest that owns this NavigationHandle.
  NavigationRequest* navigation_request_;

  base::WeakPtrFactory<NavigationHandleImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NavigationHandleImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATION_HANDLE_IMPL_H_
