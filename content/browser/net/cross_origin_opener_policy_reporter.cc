// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/cross_origin_opener_policy_reporter.h"

#include "base/values.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

constexpr char kUnsafeNone[] = "unsafe-none";
constexpr char kSameOrigin[] = "same-origin";
constexpr char kSameOriginPlusCoep[] = "same-origin-plus-coep";
constexpr char kSameOriginAllowPopups[] = "same-origin-allow-popups";

constexpr char kDisposition[] = "disposition";
constexpr char kDispositionEnforce[] = "enforce";
constexpr char kDispositionReporting[] = "reporting";
constexpr char kDocumentURI[] = "document-uri";
constexpr char kNavigationURI[] = "navigation-uri";
constexpr char kViolationType[] = "violation-type";
constexpr char kViolationTypeFromDocument[] = "navigation-from-document";
constexpr char kViolationTypeToDocument[] = "navigation-to-document";
constexpr char kEffectivePolicy[] = "effective-policy";

std::string CoopValueToString(
    network::mojom::CrossOriginOpenerPolicyValue coop_value) {
  switch (coop_value) {
    case network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone:
      return kUnsafeNone;
    case network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin:
      return kSameOrigin;
    case network::mojom::CrossOriginOpenerPolicyValue::kSameOriginAllowPopups:
      return kSameOriginAllowPopups;
    case network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep:
      return kSameOriginPlusCoep;
  }
}

RenderFrameHostImpl* GetSourceRfhForCoopReporting(
    RenderFrameHostImpl* current_rfh) {
  CHECK(current_rfh);

  // If this is a fresh popup we would consider the source RFH to be
  // our opener.
  // TODO(arthursonzogni): There seems to be no guarantee that opener() is
  // always set, do we need to be more cautious here?
  if (!current_rfh->has_committed_any_navigation())
    return current_rfh->frame_tree_node()->opener()->current_frame_host();

  // Otherwise this is simply the current RFH.
  return current_rfh;
}

base::UnguessableToken GetFrameToken(FrameTreeNode* frame,
                                     SiteInstance* site_instance) {
  RenderFrameHostImpl* rfh = frame->current_frame_host();
  if (rfh->GetSiteInstance() == site_instance)
    return rfh->GetFrameToken();

  RenderFrameProxyHost* proxy =
      frame->render_manager()->GetRenderFrameProxyHost(site_instance);
  if (proxy)
    return proxy->GetFrameToken();

  return base::UnguessableToken::Null();
}

// Find all the related windows that might try to access the new document in
// |frame|, but are in a different virtual browsing context group.
std::vector<FrameTreeNode*> CollectOtherWindowForCoopAccess(
    FrameTreeNode* frame) {
  DCHECK(frame->IsMainFrame());
  SiteInstance* site_instance = frame->current_frame_host()->GetSiteInstance();

  std::vector<FrameTreeNode*> out;
  for (WebContentsImpl* wc : WebContentsImpl::GetAllWebContents()) {
    RenderFrameHostImpl* rfh = wc->GetMainFrame();

    // Filters out windows from a different browsing context group.
    if (!rfh->GetSiteInstance()->IsRelatedSiteInstance(site_instance))
      continue;

    // TODO(arthursonzogni): Filter out window from the same virtual browsing
    // context group.
    FrameTreeNode* ftn = rfh->frame_tree_node();
    if (ftn == frame)
      continue;

    out.push_back(ftn);
  }
  return out;
}

}  // namespace

CrossOriginOpenerPolicyReporter::CrossOriginOpenerPolicyReporter(
    StoragePartition* storage_partition,
    RenderFrameHostImpl* current_rfh,
    const GURL& context_url,
    const network::CrossOriginOpenerPolicy& coop)
    : storage_partition_(storage_partition),
      context_url_(context_url),
      coop_(coop) {
  DCHECK(storage_partition_);
  RenderFrameHostImpl* source_rfh = GetSourceRfhForCoopReporting(current_rfh);
  source_url_ = source_rfh->GetLastCommittedURL();
  source_routing_id_ = source_rfh->GetGlobalFrameRoutingId();
}

CrossOriginOpenerPolicyReporter::CrossOriginOpenerPolicyReporter(
    StoragePartition* storage_partition,
    const GURL& source_url,
    const GlobalFrameRoutingId source_routing_id,
    const GURL& context_url,
    const network::CrossOriginOpenerPolicy& coop)
    : storage_partition_(storage_partition),
      source_url_(source_url),
      source_routing_id_(source_routing_id),
      context_url_(context_url),
      coop_(coop) {
  DCHECK(storage_partition_);
}

CrossOriginOpenerPolicyReporter::~CrossOriginOpenerPolicyReporter() = default;

void CrossOriginOpenerPolicyReporter::QueueOpenerBreakageReport(
    const GURL& other_url,
    bool is_reported_from_document,
    bool is_report_only) {
  const base::Optional<std::string>& endpoint =
      is_report_only ? coop_.report_only_reporting_endpoint
                     : coop_.reporting_endpoint;
  DCHECK(endpoint);

  url::Replacements<char> replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  std::string sanitized_context_url =
      context_url_.ReplaceComponents(replacements).spec();
  std::string sanitized_other_url =
      other_url.ReplaceComponents(replacements).spec();
  base::DictionaryValue body;
  body.SetString(kDisposition,
                 is_report_only ? kDispositionReporting : kDispositionEnforce);
  body.SetString(kDocumentURI, sanitized_context_url);
  body.SetString(kNavigationURI, sanitized_other_url);
  body.SetString(kViolationType, is_reported_from_document
                                     ? kViolationTypeFromDocument
                                     : kViolationTypeToDocument);
  body.SetString(kEffectivePolicy,
                 CoopValueToString(is_report_only ? coop_.report_only_value
                                                  : coop_.value));
  storage_partition_->GetNetworkContext()->QueueReport(
      "coop", *endpoint, context_url_, /*user_agent=*/base::nullopt,
      std::move(body));
}

void CrossOriginOpenerPolicyReporter::Clone(
    mojo::PendingReceiver<network::mojom::CrossOriginOpenerPolicyReporter>
        receiver) {
  receiver_set_.Add(this, std::move(receiver));
}

GURL CrossOriginOpenerPolicyReporter::GetPreviousDocumentUrlForReporting(
    const std::vector<GURL>& redirect_chain,
    const GURL& referrer_url) {
  // If the current document and all its redirect chain are same-origin with
  // the previous document, this is the previous document URL.
  auto source_origin = url::Origin::Create(source_url_);
  bool is_redirect_chain_same_origin = true;
  for (auto& redirect_url : redirect_chain) {
    auto redirect_origin = url::Origin::Create(redirect_url);
    if (!redirect_origin.IsSameOriginWith(source_origin)) {
      is_redirect_chain_same_origin = false;
      break;
    }
  }
  if (is_redirect_chain_same_origin)
    return source_url_;

  // Otherwise, it's the referrer of the navigation.
  return referrer_url;
}

GURL CrossOriginOpenerPolicyReporter::GetNextDocumentUrlForReporting(
    const std::vector<GURL>& redirect_chain,
    const GlobalFrameRoutingId& initiator_routing_id) {
  const url::Origin& source_origin = url::Origin::Create(source_url_);

  // If the next document and all its redirect chain are same-origin with the
  // current document, this is the next document URL.
  bool is_redirect_chain_same_origin = true;
  for (auto& redirect_url : redirect_chain) {
    auto redirect_origin = url::Origin::Create(redirect_url);
    if (!redirect_origin.IsSameOriginWith(source_origin)) {
      is_redirect_chain_same_origin = false;
      break;
    }
  }
  if (is_redirect_chain_same_origin)
    return redirect_chain[redirect_chain.size() - 1];

  // If the current document is the initiator of the navigation, then it's the
  // initial navigation URL.
  if (source_routing_id_ == initiator_routing_id)
    return redirect_chain[0];

  // Otherwise, it's the empty URL.
  return GURL();
}

// static
void CrossOriginOpenerPolicyReporter::InstallAccessMonitorsIfNeeded(
    FrameTreeNode* frame) {
  if (!frame->IsMainFrame())
    return;

  // The function centralize all the CoopAccessMonitor being added. Checking the
  // flag here ensures the feature to be properly disabled everywhere.
  if (!base::FeatureList::IsEnabled(
          network::features::kCrossOriginOpenerPolicyAccessReporting)) {
    return;
  }

  // TODO(arthursonzogni): It is too late to update the SiteInstance of the new
  // document. Ideally, this should be split into two parts:
  // - CommitNavigation: Update the new document's SiteInstance.
  // - DidCommitNavigation: Update the other SiteInstances.

  // Find all the related windows that might try to access the new document,
  // but are from a different virtual browsing context group.
  std::vector<FrameTreeNode*> other_main_frames =
      CollectOtherWindowForCoopAccess(frame);

  CrossOriginOpenerPolicyReporter* reporter_frame =
      frame->current_frame_host()->coop_reporter();

  for (FrameTreeNode* other : other_main_frames) {
    CrossOriginOpenerPolicyReporter* reporter_other =
        other->current_frame_host()->coop_reporter();

    // If the current frame has a reporter, install the access monitors to
    // monitor the accesses between this frame and the other frame.
    if (reporter_frame) {
      reporter_frame->MonitorAccesses(frame, other);
      reporter_frame->MonitorAccesses(other, frame);
    }

    // If the other frame has a reporter, install the access monitors to monitor
    // the accesses between this frame and the other frame.
    if (reporter_other) {
      reporter_other->MonitorAccesses(frame, other);
      reporter_other->MonitorAccesses(other, frame);
    }
  }
}

void CrossOriginOpenerPolicyReporter::MonitorAccesses(
    FrameTreeNode* accessing_node,
    FrameTreeNode* accessed_node) {
  DCHECK_NE(accessing_node, accessed_node);
  DCHECK(accessing_node->current_frame_host()->coop_reporter() == this ||
         accessed_node->current_frame_host()->coop_reporter() == this);

  // TODO(arthursonzogni): DCHECK same browsing context group.
  // TODO(arthursonzogni): DCHECK different virtual browsing context group.

  // Accesses are made either from the main frame or its same-origin iframes.
  // Accesses from the cross-origin ones aren't reported.
  //
  // It means all the accessed from the first window are made from documents
  // inside the same SiteInstance. Only one SiteInstance has to be updated.

  RenderFrameHostImpl* accessing_rfh = accessing_node->current_frame_host();
  SiteInstance* site_instance = accessing_rfh->GetSiteInstance();

  base::UnguessableToken accessed_window_token =
      GetFrameToken(accessed_node, site_instance);
  if (!accessed_window_token)
    return;

  mojo::PendingRemote<network::mojom::CrossOriginOpenerPolicyReporter>
      remote_reporter;
  Clone(remote_reporter.InitWithNewPipeAndPassReceiver());

  accessing_rfh->GetAssociatedLocalMainFrame()->InstallCoopAccessMonitor(
      accessed_window_token, std::move(remote_reporter));
}

}  // namespace content
