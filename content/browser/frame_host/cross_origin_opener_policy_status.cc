// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/cross_origin_opener_policy_status.h"

#include <utility>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"

namespace content {

namespace {

// This function implements the COOP matching algorithm as detailed in [1].
// Note that COEP is also provided since the COOP enum does not have a
// "same-origin + COEP" value.
// [1] https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e
bool CrossOriginOpenerPolicyMatch(
    network::mojom::CrossOriginOpenerPolicyValue initiator_coop,
    const url::Origin& initiator_origin,
    network::mojom::CrossOriginOpenerPolicyValue destination_coop,
    const url::Origin& destination_origin) {
  if (initiator_coop != destination_coop)
    return false;

  if (initiator_coop ==
      network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone) {
    return true;
  }

  if (!initiator_origin.IsSameOriginWith(destination_origin))
    return false;
  return true;
}

// This function returns whether the BrowsingInstance should change following
// COOP rules defined in:
// https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e#changes-to-navigation
bool ShouldSwapBrowsingInstanceForCrossOriginOpenerPolicy(
    network::mojom::CrossOriginOpenerPolicyValue initiator_coop,
    const url::Origin& initiator_origin,
    bool is_initial_navigation,
    network::mojom::CrossOriginOpenerPolicyValue destination_coop,
    const url::Origin& destination_origin) {
  using network::mojom::CrossOriginOpenerPolicyValue;

  // If policies match there is no reason to switch BrowsingInstances.
  if (CrossOriginOpenerPolicyMatch(initiator_coop, initiator_origin,
                                   destination_coop, destination_origin)) {
    return false;
  }

  // "same-origin-allow-popups" is used to stay in the same BrowsingInstance
  // despite COOP mismatch. This case is defined in the spec [1] as follow.
  // ```
  // If the result of matching currentCOOP, currentOrigin, potentialCOOP, and
  // potentialOrigin is false and one of the following is false:
  //  - doc is the initial about:blank document
  //  - currentCOOP is "same-origin-allow-popups"
  //  - potentialCOOP is "unsafe-none"
  // Then create a new browsing context group.
  // ```
  // [1]
  // https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e#changes-to-navigation
  if (is_initial_navigation &&
      initiator_coop == CrossOriginOpenerPolicyValue::kSameOriginAllowPopups &&
      destination_coop == CrossOriginOpenerPolicyValue::kUnsafeNone) {
    return false;
  }
  return true;
}

}  // namespace

CrossOriginOpenerPolicyStatus::CrossOriginOpenerPolicyStatus(
    FrameTreeNode* frame_tree_node)
    : frame_tree_node_(frame_tree_node),
      virtual_browsing_context_group_(frame_tree_node->current_frame_host()
                                          ->virtual_browsing_context_group()),
      had_opener_(!!frame_tree_node->opener()),
      is_initial_navigation_(!frame_tree_node_->has_committed_real_load()),
      current_coop_(
          frame_tree_node->current_frame_host()->cross_origin_opener_policy()),
      current_origin_(
          frame_tree_node->current_frame_host()->GetLastCommittedOrigin()) {}

CrossOriginOpenerPolicyStatus::~CrossOriginOpenerPolicyStatus() = default;

base::Optional<network::mojom::BlockedByResponseReason>
CrossOriginOpenerPolicyStatus::EnforceCOOP(
    network::mojom::URLResponseHead* response_head,
    const url::Origin& response_origin,
    const GURL& response_url) {
  SanitizeCoopHeaders(response_url, response_origin, response_head);
  network::mojom::ParsedHeaders* parsed_headers =
      response_head->parsed_headers.get();

  // Return early if the situation prevents COOP from operating.
  if (!frame_tree_node_->IsMainFrame() ||
      response_url.IsAboutBlank()) {
    return base::nullopt;
  }

  network::CrossOriginOpenerPolicy& response_coop =
      parsed_headers->cross_origin_opener_policy;

  // Popups with a sandboxing flag, inherited from their opener, are not
  // allowed to navigate to a document with a Cross-Origin-Opener-Policy that
  // is not "unsafe-none". This ensures a COOP document does not inherit any
  // property from an opener.
  // https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e
  if (response_coop.value !=
          network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone &&
      (frame_tree_node_->pending_frame_policy().sandbox_flags !=
       network::mojom::WebSandboxFlags::kNone)) {
    return network::mojom::BlockedByResponseReason::
        kCoopSandboxedIFrameCannotNavigateToCoopPage;
  }

  bool cross_origin_policy_swap =
      ShouldSwapBrowsingInstanceForCrossOriginOpenerPolicy(
          current_coop_.value, current_origin_, is_initial_navigation_,
          response_coop.value, response_origin);

  // Both report only cases (navigation from and to document) use the following
  // result, computing the need of a browsing context group swap based on both
  // documents' report-only values.
  bool report_only_coop_swap =
      ShouldSwapBrowsingInstanceForCrossOriginOpenerPolicy(
          current_coop_.report_only_value, current_origin_,
          is_initial_navigation_, response_coop.report_only_value,
          response_origin);

  bool navigating_to_report_only_coop_swap =
      ShouldSwapBrowsingInstanceForCrossOriginOpenerPolicy(
          current_coop_.value, current_origin_, is_initial_navigation_,
          response_coop.report_only_value, response_origin);

  bool navigating_from_report_only_coop_swap =
      ShouldSwapBrowsingInstanceForCrossOriginOpenerPolicy(
          current_coop_.report_only_value, current_origin_,
          is_initial_navigation_, response_coop.value, response_origin);

  if (cross_origin_policy_swap)
    require_browsing_instance_swap_ = true;

  if (report_only_coop_swap && (navigating_to_report_only_coop_swap ||
                                navigating_from_report_only_coop_swap)) {
    virtual_browsing_instance_swap_ = true;
  }

  if (require_browsing_instance_swap_ || virtual_browsing_instance_swap_) {
    virtual_browsing_context_group_ =
        CrossOriginOpenerPolicyReporter::NextVirtualBrowsingContextGroup();
  }

  // Finally, update the current COOP, origin and reporter to those of the
  // response, now that it has been taken into account.
  current_coop_ = response_coop;
  current_origin_ = response_origin;

  return base::nullopt;
}

// We blank out the COOP headers in a number of situations.
// - When the headers were not sent over HTTPS.
// - For subframes.
// - When the feature is disabled.
// We also strip the "reporting" parts when the reporting feature is disabled
// for the |response_origin|.
void CrossOriginOpenerPolicyStatus::SanitizeCoopHeaders(
    const GURL& response_url,
    const url::Origin& response_origin,
    network::mojom::URLResponseHead* response_head) {
  network::CrossOriginOpenerPolicy& coop =
      response_head->parsed_headers->cross_origin_opener_policy;
  if (coop == network::CrossOriginOpenerPolicy())
    return;

  if (!base::FeatureList::IsEnabled(
          network::features::kCrossOriginOpenerPolicy) ||
      // https://html.spec.whatwg.org/multipage#the-cross-origin-opener-policy-header
      // ```
      // 1. If reservedEnvironment is a non-secure context, then return
      //    "unsafe-none".
      // ```
      !network::IsOriginPotentiallyTrustworthy(response_origin) ||
      // The COOP header must be ignored outside of the top-level context. It is
      // removed as a defensive measure.
      !frame_tree_node_->IsMainFrame()) {
    coop = network::CrossOriginOpenerPolicy();

    if (!network::IsOriginPotentiallyTrustworthy(response_origin))
      header_ignored_due_to_insecure_context_ = true;
    return;
  }

  // The reporting part can be enabled via either a command-line flag or an
  // origin trial.
  bool reporting_enabled = base::FeatureList::IsEnabled(
      network::features::kCrossOriginOpenerPolicyReporting);

  reporting_enabled |=
      base::FeatureList::IsEnabled(
          network::features::kCrossOriginOpenerPolicyReportingOriginTrial) &&
      blink::TrialTokenValidator().RequestEnablesFeature(
          response_url, response_head->headers.get(),
          "CrossOriginOpenerPolicyReporting", base::Time::Now());

  if (!reporting_enabled) {
    coop.reporting_endpoint = base::nullopt;
    coop.report_only_reporting_endpoint = base::nullopt;
    coop.report_only_value =
        network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone;
  }
}

}  // namespace content
