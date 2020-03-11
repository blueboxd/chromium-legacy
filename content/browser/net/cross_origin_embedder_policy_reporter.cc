// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/cross_origin_embedder_policy_reporter.h"

#include "base/values.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace content {

CrossOriginEmbedderPolicyReporter::CrossOriginEmbedderPolicyReporter(
    StoragePartition* storage_partition,
    const GURL& context_url,
    const base::Optional<std::string>& endpoint,
    const base::Optional<std::string>& report_only_endpoint)
    : storage_partition_(storage_partition),
      context_url_(context_url),
      endpoint_(endpoint),
      report_only_endpoint_(report_only_endpoint) {
  DCHECK(storage_partition_);
}

CrossOriginEmbedderPolicyReporter::~CrossOriginEmbedderPolicyReporter() =
    default;

void CrossOriginEmbedderPolicyReporter::QueueCorpViolationReport(
    const GURL& blocked_url,
    bool report_only) {
  const base::Optional<std::string>& endpoint =
      report_only ? report_only_endpoint_ : endpoint_;
  if (!endpoint) {
    return;
  }
  url::Replacements<char> replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  base::DictionaryValue body;
  body.SetString("type", "corp");
  body.SetString("blocked-url",
                 blocked_url.ReplaceComponents(replacements).spec());
  storage_partition_->GetNetworkContext()->QueueReport(
      "coep", *endpoint, context_url_, /*user_agent=*/base::nullopt,
      std::move(body));
}

void CrossOriginEmbedderPolicyReporter::QueueNavigationReport(
    const GURL& blocked_url,
    bool report_only) {
  const base::Optional<std::string>& endpoint =
      report_only ? report_only_endpoint_ : endpoint_;
  if (!endpoint) {
    return;
  }
  url::Replacements<char> replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  base::DictionaryValue body;
  body.SetString("type", "navigation");
  body.SetString("blocked-url",
                 blocked_url.ReplaceComponents(replacements).spec());
  storage_partition_->GetNetworkContext()->QueueReport(
      "coep", *endpoint, context_url_, /*user_agent=*/base::nullopt,
      std::move(body));
}

void CrossOriginEmbedderPolicyReporter::Clone(
    mojo::PendingReceiver<network::mojom::CrossOriginEmbedderPolicyReporter>
        receiver) {
  receiver_set_.Add(this, std::move(receiver));
}

}  // namespace content
