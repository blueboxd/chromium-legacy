// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_redirect/https_image_compression_bypass_decider.h"

#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "third_party/blink/public/common/features.h"

HttpsImageCompressionBypassDecider::HttpsImageCompressionBypassDecider() =
    default;

HttpsImageCompressionBypassDecider::~HttpsImageCompressionBypassDecider() =
    default;

bool HttpsImageCompressionBypassDecider::ShouldBypassNow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::FeatureList::IsEnabled(blink::features::kSubresourceRedirect));
  bool should_bypass =
      bypassed_until_time_ && base::TimeTicks::Now() <= bypassed_until_time_;
  UMA_HISTOGRAM_BOOLEAN("SubresourceRedirect.PageLoad.BypassResult",
                        should_bypass);
  return should_bypass;
}

void HttpsImageCompressionBypassDecider::NotifyCompressedImageFetchFailed(
    base::TimeDelta retry_after) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::FeatureList::IsEnabled(blink::features::kSubresourceRedirect));
  if (bypassed_until_time_)
    return;  // Bypass is already enabled due to a previous failure.

  if (!retry_after.is_zero()) {
    // Choose the time mentioned in retry_after, but cap it to 5 minutes.
    retry_after = std::min(retry_after, base::TimeDelta::FromMinutes(5));
  } else {
    // Bypass for a random duration between 1 to 5 minutes.
    retry_after = base::TimeDelta::FromSeconds(base::RandInt(1 * 60, 5 * 60));
  }
  bypassed_until_time_ = base::TimeTicks::Now() + retry_after;
  UMA_HISTOGRAM_LONG_TIMES("SubresourceRedirect.BypassDuration", retry_after);
}
