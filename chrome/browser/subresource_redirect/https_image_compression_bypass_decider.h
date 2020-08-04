// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBRESOURCE_REDIRECT_HTTPS_IMAGE_COMPRESSION_BYPASS_DECIDER_H_
#define CHROME_BROWSER_SUBRESOURCE_REDIRECT_HTTPS_IMAGE_COMPRESSION_BYPASS_DECIDER_H_

#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"

// Interface to decide whether https image compression should be bypassed for a
// page load. Whenever an image fetch for compression server fails, image
// compression feature is turned off for a random 1-5 minute duration or until
// the time mentioned in Retry-After response header from compression server.
class HttpsImageCompressionBypassDecider {
 public:
  HttpsImageCompressionBypassDecider();
  ~HttpsImageCompressionBypassDecider();

  // Returns whether https image compression should be bypassed for the current
  // page load. Should be called only once per page load, since it records
  // histograms that is expected once per page load.
  bool ShouldBypassNow();

  // Notifies the decider that an image compression fetch had failed, which will
  // start bypassing image compression feature for subsequent pageloads.
  void NotifyCompressedImageFetchFailed(base::TimeDelta retry_after);

  base::Optional<base::TimeTicks> GetBypassUntilTimeForTesting() const {
    return bypassed_until_time_;
  }
  void SetBypassUntilTimeForTesting(base::TimeTicks bypass_until) {
    bypassed_until_time_ = bypass_until;
  }

 private:
  // The time until which image compression should be bypassed. Null time
  // indicates no bypass.
  base::Optional<base::TimeTicks> bypassed_until_time_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_SUBRESOURCE_REDIRECT_HTTPS_IMAGE_COMPRESSION_BYPASS_DECIDER_H_
