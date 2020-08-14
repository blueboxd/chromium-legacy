// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_CROSS_ORIGIN_OPENER_POLICY_STATUS_H_
#define CONTENT_BROWSER_FRAME_HOST_CROSS_ORIGIN_OPENER_POLICY_STATUS_H_

#include <memory>

#include "services/network/public/cpp/cross_origin_opener_policy.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace content {
class FrameTreeNode;

// Groups information used to apply COOP during navigations. This class will be
// used to trigger a number of mechanisms such as BrowsingInstance switch or
// reporting.
class CrossOriginOpenerPolicyStatus {
 public:
  explicit CrossOriginOpenerPolicyStatus(FrameTreeNode* frame_tree_node);
  ~CrossOriginOpenerPolicyStatus();

  // Called after receiving a network response. Returns a BlockedByResponse
  // reason if the navigation should be blocked, nullopt otherwise.
  base::Optional<network::mojom::BlockedByResponseReason> EnforceCOOP(
      network::mojom::ParsedHeaders* parsed_headers,
      const url::Origin& response_origin,
      const GURL& response_url);

  // Set to true whenever the Cross-Origin-Opener-Policy spec requires a
  // "BrowsingContext group" swap:
  // https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e
  // This forces the new RenderFrameHost to use a different BrowsingInstance
  // than the current one. If other pages had JavaScript references to the
  // Window object for the frame (via window.opener, window.open(), et cetera),
  // those references will be broken; window.name will also be reset to an empty
  // string.
  bool require_browsing_instance_swap() const {
    return require_browsing_instance_swap_;
  }

  // As detailed in
  // https://github.com/camillelamy/explainers/blob/master/coop_reporting.md#browsing-context-changes:
  // Set to true when the Cross-Origin-Opener-Policy-Report-Only value of the
  // involved documents would cause a browsing context group swap.
  bool virtual_browsing_instance_swap() const {
    return virtual_browsing_instance_swap_;
  }

  // The virtual browsing context group of the document to commit. Initially,
  // the navigation inherits the virtual browsing context group of the current
  // document. Updated when the report-only COOP of a response would result in
  // a browsing context group swap if enforced.
  int virtual_browsing_context_group() const {
    return virtual_browsing_context_group_;
  }

  // When a page has a reachable opener and COOP triggers a browsing instance
  // swap we sever the window.open relationship. This is one of the cases that
  // can be reported using the COOP reporting API.
  bool had_opener() const { return had_opener_; }

  // This is used to warn developer a COOP header has been ignored, because
  // the origin was not trustworthy.
  bool header_ignored_due_to_insecure_context() const {
    return header_ignored_due_to_insecure_context_;
  }

  // The COOP used when comparing to the COOP and origin of a response. At the
  // beginning of the navigation, it is the COOP of the current document. After
  // receiving any kind of response, including redirects, it is the COOP of the
  // last response.
  const network::CrossOriginOpenerPolicy& current_coop() const {
    return current_coop_;
  }

 private:
  // Make sure COOP is relevant or clear the COOP headers.
  void SanitizeCoopHeaders(const url::Origin& response_origin,
                           network::mojom::ParsedHeaders* parsed_headers);

  // Tracks the FrameTreeNode in which this navigation is taking place.
  const FrameTreeNode* frame_tree_node_;

  bool require_browsing_instance_swap_ = false;
  bool virtual_browsing_instance_swap_ = false;
  int virtual_browsing_context_group_;
  const bool had_opener_;

  // Whether this is the first navigation happening in the browsing context.
  const bool is_initial_navigation_;

  bool header_ignored_due_to_insecure_context_ = false;

  network::CrossOriginOpenerPolicy current_coop_;

  // The origin used when comparing to the COOP and origin of a response. At
  // the beginning of the navigation, it is the origin of the current document.
  // After receiving any kind of response, including redirects, it is the origin
  // of the last response.
  url::Origin current_origin_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_CROSS_ORIGIN_OPENER_POLICY_STATUS_H_
