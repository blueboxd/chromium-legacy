// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NET_CROSS_ORIGIN_OPENER_POLICY_REPORTER_H_
#define CONTENT_BROWSER_NET_CROSS_ORIGIN_OPENER_POLICY_REPORTER_H_

#include <string>

#include "base/optional.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom.h"
#include "services/network/public/mojom/cross_origin_opener_policy.mojom.h"
#include "url/gurl.h"

namespace content {

class StoragePartition;
class RenderFrameHostImpl;

// Used to report (potential) COOP breakages.
// A CrossOriginOpenerPolicyReporter lives in the browser process and is either
// held by the NavigationRequest during navigation or by the RenderFrameHostImpl
// after the document has committed.
// To make calls from other processes, create a mojo endpoint using Clone and
// pass the receiver to other processes.
// Any functions other than the destructor must not be called after the
// associated StoragePartition is destructed.
class CONTENT_EXPORT CrossOriginOpenerPolicyReporter final
    : public network::mojom::CrossOriginOpenerPolicyReporter {
 public:
  CrossOriginOpenerPolicyReporter(
      StoragePartition* storage_partition,
      RenderFrameHostImpl* current_frame_host,
      const GURL& context_url,
      const network::CrossOriginOpenerPolicy& coop,
      const network::CrossOriginEmbedderPolicy& coep);
  ~CrossOriginOpenerPolicyReporter() override;
  CrossOriginOpenerPolicyReporter(const CrossOriginOpenerPolicyReporter&) =
      delete;
  CrossOriginOpenerPolicyReporter& operator=(
      const CrossOriginOpenerPolicyReporter&) = delete;

  // network::mojom::CrossOriginOpenerPolicyReporter implementation.
  void QueueOpenerBreakageReport(const GURL& other_url,
                                 bool is_reported_from_document,
                                 bool is_report_only) override;

  // Returns the "previous" URL that is safe to expose.
  // Reference, "Next document URL for reporting" section:
  // https://github.com/camillelamy/explainers/blob/master/coop_reporting.md#safe-urls-for-reporting
  GURL GetPreviousDocumentUrlForReporting(
      const std::vector<GURL>& redirect_chain,
      const GURL& referrer_url);

  // Returns the "next" URL that is safe to expose.
  // Reference, "Next document URL for reporting" section:
  // https://github.com/camillelamy/explainers/blob/master/coop_reporting.md#safe-urls-for-reporting
  GURL GetNextDocumentUrlForReporting(
      const std::vector<GURL>& redirect_chain,
      const GlobalFrameRoutingId& initiator_routing_id);

  void Clone(
      mojo::PendingReceiver<network::mojom::CrossOriginOpenerPolicyReporter>
          receiver) override;

 private:
  friend class CrossOriginOpenerPolicyReporterTest;

  // Used in unit_tests that do not have access to a RenderFrameHost.
  CrossOriginOpenerPolicyReporter(
      StoragePartition* storage_partition,
      const GURL& source_url,
      const GlobalFrameRoutingId source_routing_id,
      const GURL& context_url,
      const network::CrossOriginOpenerPolicy& coop,
      const network::CrossOriginEmbedderPolicy& coep);

  // See the class comment.
  StoragePartition* const storage_partition_;
  GURL source_url_;
  GlobalFrameRoutingId source_routing_id_;
  const GURL context_url_;
  network::CrossOriginOpenerPolicy coop_;
  network::CrossOriginEmbedderPolicy coep_;

  mojo::ReceiverSet<network::mojom::CrossOriginOpenerPolicyReporter>
      receiver_set_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_NET_CROSS_ORIGIN_OPENER_POLICY_REPORTER_H_
