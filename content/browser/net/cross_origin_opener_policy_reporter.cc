// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/cross_origin_opener_policy_reporter.h"

#include "base/values.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/network_context.mojom.h"

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
    network::mojom::CrossOriginOpenerPolicyValue coop_value,
    network::mojom::CrossOriginEmbedderPolicyValue coep_value,
    network::mojom::CrossOriginEmbedderPolicyValue report_only_coep_value) {
  switch (coop_value) {
    case network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone:
      return kUnsafeNone;
    case network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin:
      if ((coep_value ==
           network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp) ||
          (report_only_coep_value ==
           network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp)) {
        return kSameOriginPlusCoep;
      }
      return kSameOrigin;
    case network::mojom::CrossOriginOpenerPolicyValue::kSameOriginAllowPopups:
      return kSameOriginAllowPopups;
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

}  // namespace

CrossOriginOpenerPolicyReporter::CrossOriginOpenerPolicyReporter(
    StoragePartition* storage_partition,
    RenderFrameHostImpl* current_rfh,
    const GURL& context_url,
    const network::CrossOriginOpenerPolicy& coop,
    const network::CrossOriginEmbedderPolicy& coep)
    : storage_partition_(storage_partition),
      context_url_(context_url),
      coop_(coop),
      coep_(coep) {
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
    const network::CrossOriginOpenerPolicy& coop,
    const network::CrossOriginEmbedderPolicy& coep)
    : storage_partition_(storage_partition),
      source_url_(source_url),
      source_routing_id_(source_routing_id),
      context_url_(context_url),
      coop_(coop),
      coep_(coep) {
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
  body.SetString(
      kEffectivePolicy,
      CoopValueToString(is_report_only ? coop_.report_only_value : coop_.value,
                        coep_.value, coep_.report_only_value));
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

}  // namespace content
