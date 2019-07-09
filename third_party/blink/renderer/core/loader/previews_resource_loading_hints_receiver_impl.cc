// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/previews_resource_loading_hints_receiver_impl.h"

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/previews_resource_loading_hints.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

PreviewsResourceLoadingHintsReceiverImpl::
    PreviewsResourceLoadingHintsReceiverImpl(
        mojom::blink::PreviewsResourceLoadingHintsReceiverRequest request,
        Document* document)
    : binding_(this, std::move(request)), document_(document) {
  DCHECK(!base::FeatureList::IsEnabled(
      blink::features::kSendPreviewsLoadingHintsBeforeCommit));
}

PreviewsResourceLoadingHintsReceiverImpl::
    ~PreviewsResourceLoadingHintsReceiverImpl() {}

void PreviewsResourceLoadingHintsReceiverImpl::SetResourceLoadingHints(
    mojom::blink::PreviewsResourceLoadingHintsPtr resource_loading_hints) {
  // TODO(tbansal): https://crbug.com/856247. Block loading of resources based
  // on |resource_loading_hints|.
  UMA_HISTOGRAM_COUNTS_100(
      "ResourceLoadingHints.CountBlockedSubresourcePatterns",
      resource_loading_hints->subresources_to_block.size());

  Vector<WTF::String> subresource_patterns_to_block;
  for (const auto& subresource :
       resource_loading_hints->subresources_to_block) {
    subresource_patterns_to_block.push_back(subresource);
  }

  document_->Loader()->SetPreviewsResourceLoadingHints(
      PreviewsResourceLoadingHints::Create(
          *(document_.Get()), resource_loading_hints->ukm_source_id,
          subresource_patterns_to_block));
}

}  // namespace blink
