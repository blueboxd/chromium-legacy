// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/renderer/subresource_filter_agent.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/subresource_filter/content/common/subresource_filter_messages.h"
#include "components/subresource_filter/content/common/subresource_filter_utils.h"
#include "components/subresource_filter/content/renderer/unverified_ruleset_dealer.h"
#include "components/subresource_filter/content/renderer/web_document_subresource_filter_impl.h"
#include "components/subresource_filter/core/common/document_subresource_filter.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/subresource_filter/core/common/scoped_timers.h"
#include "components/subresource_filter/core/common/time_measurements.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "ipc/ipc_message.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/url_constants.h"

namespace subresource_filter {

SubresourceFilterAgent::SubresourceFilterAgent(
    content::RenderFrame* render_frame,
    UnverifiedRulesetDealer* ruleset_dealer,
    std::unique_ptr<AdResourceTracker> ad_resource_tracker)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<SubresourceFilterAgent>(render_frame),
      ruleset_dealer_(ruleset_dealer),
      ad_resource_tracker_(std::move(ad_resource_tracker)) {
  DCHECK(ruleset_dealer);
  // |render_frame| can be nullptr in unit tests.
  if (render_frame) {
    render_frame->GetAssociatedInterfaceRegistry()->AddInterface(
        base::BindRepeating(
            &SubresourceFilterAgent::OnSubresourceFilterAgentRequest,
            base::Unretained(this)));
  }
}

SubresourceFilterAgent::~SubresourceFilterAgent() {
  // Filter may outlive us, so reset the ad tracker.
  if (filter_for_last_created_document_)
    filter_for_last_created_document_->set_ad_resource_tracker(nullptr);
}

GURL SubresourceFilterAgent::GetDocumentURL() {
  return render_frame()->GetWebFrame()->GetDocument().Url();
}

bool SubresourceFilterAgent::IsMainFrame() {
  return render_frame()->IsMainFrame();
}

bool SubresourceFilterAgent::HasDocumentLoader() {
  return render_frame()->GetWebFrame()->GetDocumentLoader();
}

void SubresourceFilterAgent::SetSubresourceFilterForCurrentDocument(
    std::unique_ptr<blink::WebDocumentSubresourceFilter> filter) {
  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  DCHECK(web_frame->GetDocumentLoader());
  web_frame->GetDocumentLoader()->SetSubresourceFilter(filter.release());
}

void SubresourceFilterAgent::
    SignalFirstSubresourceDisallowedForCurrentDocument() {
  GetSubresourceFilterHost()->DidDisallowFirstSubresource();
}

void SubresourceFilterAgent::SendDocumentLoadStatistics(
    const mojom::DocumentLoadStatistics& statistics) {
  GetSubresourceFilterHost()->SetDocumentLoadStatistics(statistics.Clone());
}

void SubresourceFilterAgent::SendFrameIsAdSubframe() {
  GetSubresourceFilterHost()->FrameIsAdSubframe();
}

bool SubresourceFilterAgent::IsAdSubframe() {
  return render_frame()->GetWebFrame()->IsAdSubframe();
}

void SubresourceFilterAgent::SetIsAdSubframe(
    blink::mojom::AdFrameType ad_frame_type) {
  render_frame()->GetWebFrame()->SetIsAdSubframe(ad_frame_type);
}

mojom::ActivationState SubresourceFilterAgent::GetParentActivationState(
    content::RenderFrame* render_frame) {
  blink::WebFrame* parent =
      render_frame ? render_frame->GetWebFrame()->Parent() : nullptr;
  if (parent && parent->IsWebLocalFrame()) {
    auto* agent = SubresourceFilterAgent::Get(
        content::RenderFrame::FromWebFrame(parent->ToWebLocalFrame()));
    if (agent && agent->filter_for_last_created_document_)
      return agent->filter_for_last_created_document_->activation_state();
  }
  return mojom::ActivationState();
}

void SubresourceFilterAgent::RecordHistogramsOnFilterCreation(
    const mojom::ActivationState& activation_state) {
  // Note: mojom::ActivationLevel used to be called mojom::ActivationState, the
  // legacy name is kept for the histogram.
  mojom::ActivationLevel activation_level = activation_state.activation_level;
  UMA_HISTOGRAM_ENUMERATION("SubresourceFilter.DocumentLoad.ActivationState",
                            activation_level);

  if (IsMainFrame()) {
    UMA_HISTOGRAM_BOOLEAN(
        "SubresourceFilter.MainFrameLoad.RulesetIsAvailableAnyActivationLevel",
        ruleset_dealer_->IsRulesetFileAvailable());
  }
  if (activation_level != mojom::ActivationLevel::kDisabled) {
    UMA_HISTOGRAM_BOOLEAN("SubresourceFilter.DocumentLoad.RulesetIsAvailable",
                          ruleset_dealer_->IsRulesetFileAvailable());
  }
}

void SubresourceFilterAgent::ResetInfoForNextDocument() {
  activation_state_for_next_document_ = mojom::ActivationState();
}

mojom::SubresourceFilterHost*
SubresourceFilterAgent::GetSubresourceFilterHost() {
  if (!subresource_filter_host_) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        &subresource_filter_host_);
  }
  return subresource_filter_host_.get();
}

void SubresourceFilterAgent::OnSubresourceFilterAgentRequest(
    mojo::PendingAssociatedReceiver<mojom::SubresourceFilterAgent> receiver) {
  receiver_.Bind(std::move(receiver));
}

void SubresourceFilterAgent::ActivateForNextCommittedLoad(
    mojom::ActivationStatePtr activation_state,
    blink::mojom::AdFrameType ad_frame_type) {
  activation_state_for_next_document_ = *activation_state;
  if (ad_frame_type != blink::mojom::AdFrameType::kNonAd) {
    SetIsAdSubframe(ad_frame_type);
  }
}

void SubresourceFilterAgent::OnDestruct() {
  delete this;
}

void SubresourceFilterAgent::DidCreateNewDocument() {
  // TODO(csharrison): Use WebURL and WebSecurityOrigin for efficiency here and
  // in MaybeCreateFilterForDocument, which requires changes to the unit tests.
  const GURL& url = GetDocumentURL();
  if (first_document_) {
    first_document_ = false;
    DCHECK(!filter_for_last_created_document_);

    // Local subframes will first create an initial empty document (with url
    // kAboutBlankURL) no matter what the src is set to (or if it is not set).
    // There is then a second document created with url equal to the set src (or
    // kAboutBlankURL if unset). See DidFailProvisionalLoad for handling a
    // failure in the load of this second document. Frames created by the
    // browser initialize the LocalFrame before creating RenderFrameObservers,
    // so the initial empty document isn't observed. We only care about local
    // subframes.
    if (url == url::kAboutBlankURL) {
      if (IsAdSubframe())
        SendFrameIsAdSubframe();
      return;
    }
  }

  MaybeCreateFilterForDocument(ShouldUseParentActivation(url));
}

void SubresourceFilterAgent::MaybeCreateFilterForDocument(
    bool should_use_parent_activation) {
  // Filter may outlive us, so reset the ad tracker.
  if (filter_for_last_created_document_)
    filter_for_last_created_document_->set_ad_resource_tracker(nullptr);
  filter_for_last_created_document_.reset();

  const GURL& url = GetDocumentURL();

  const mojom::ActivationState activation_state =
      (!IsMainFrame() && should_use_parent_activation)
          ? GetParentActivationState(render_frame())
          : activation_state_for_next_document_;

  ResetInfoForNextDocument();

  // Do not pollute the histograms for empty main frame documents.
  if (IsMainFrame() && !url.SchemeIsHTTPOrHTTPS() && !url.SchemeIsFile())
    return;

  RecordHistogramsOnFilterCreation(activation_state);
  if (activation_state.activation_level == mojom::ActivationLevel::kDisabled ||
      !ruleset_dealer_->IsRulesetFileAvailable())
    return;

  scoped_refptr<const MemoryMappedRuleset> ruleset =
      ruleset_dealer_->GetRuleset();
  if (!ruleset)
    return;

  base::OnceClosure first_disallowed_load_callback(
      base::BindOnce(&SubresourceFilterAgent::
                         SignalFirstSubresourceDisallowedForCurrentDocument,
                     AsWeakPtr()));
  auto filter = std::make_unique<WebDocumentSubresourceFilterImpl>(
      url::Origin::Create(url), activation_state, std::move(ruleset),
      std::move(first_disallowed_load_callback));
  filter->set_ad_resource_tracker(ad_resource_tracker_.get());
  filter_for_last_created_document_ = filter->AsWeakPtr();
  SetSubresourceFilterForCurrentDocument(std::move(filter));
}

void SubresourceFilterAgent::DidFailProvisionalLoad() {
  // TODO(engedy): Add a test with `frame-ancestor` violation to exercise this.
  ResetInfoForNextDocument();

  // This is necessary to account for when an initial frame load is aborted,
  // for example due to a document.write or window.stop. We skip this if
  // |filter_for_last_created_document_|, maintaining the prior behavior when a
  // filter has already been created. We also skip if there is no document
  // loader as it may have been detached due to a cross-origin navigation.
  if (!filter_for_last_created_document_ && HasDocumentLoader()) {
    // Use the parent as a committed load has not occurred so an activation
    // state won't have been sent.
    MaybeCreateFilterForDocument(true);
  }
}

void SubresourceFilterAgent::DidFinishLoad() {
  if (!filter_for_last_created_document_)
    return;
  const auto& statistics =
      filter_for_last_created_document_->filter().statistics();
  SendDocumentLoadStatistics(statistics);
}

void SubresourceFilterAgent::WillCreateWorkerFetchContext(
    blink::WebWorkerFetchContext* worker_fetch_context) {
  if (!filter_for_last_created_document_)
    return;
  if (!ruleset_dealer_->IsRulesetFileAvailable())
    return;
  base::File ruleset_file = ruleset_dealer_->DuplicateRulesetFile();
  if (!ruleset_file.IsValid())
    return;

  worker_fetch_context->SetSubresourceFilterBuilder(
      std::make_unique<WebDocumentSubresourceFilterImpl::BuilderImpl>(
          url::Origin::Create(GetDocumentURL()),
          filter_for_last_created_document_->filter().activation_state(),
          std::move(ruleset_file),
          base::BindOnce(&SubresourceFilterAgent::
                             SignalFirstSubresourceDisallowedForCurrentDocument,
                         AsWeakPtr())));
}

}  // namespace subresource_filter
