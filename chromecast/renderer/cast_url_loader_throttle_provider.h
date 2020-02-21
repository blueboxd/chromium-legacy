// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_CAST_URL_LOADER_THROTTLE_PROVIDER_H_
#define CHROMECAST_RENDERER_CAST_URL_LOADER_THROTTLE_PROVIDER_H_

#include <vector>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "chromecast/common/activity_url_filter.h"
#include "chromecast/renderer/cast_activity_url_filter_manager.h"
#include "content/public/renderer/url_loader_throttle_provider.h"

namespace chromecast {

class CastURLLoaderThrottleProvider
    : public content::URLLoaderThrottleProvider {
 public:
  explicit CastURLLoaderThrottleProvider(
      content::URLLoaderThrottleProviderType type,
      CastActivityUrlFilterManager* url_filter_manager);
  ~CastURLLoaderThrottleProvider() override;

  // content::URLLoaderThrottleProvider implementation:
  std::unique_ptr<content::URLLoaderThrottleProvider> Clone() override;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> CreateThrottles(
      int render_frame_id,
      const blink::WebURLRequest& request) override;
  void SetOnline(bool is_online) override;

 protected:
  content::URLLoaderThrottleProviderType type_;
  CastActivityUrlFilterManager* const cast_activity_url_filter_manager_;

 private:
  // This copy constructor works in conjunction with Clone(), not intended for
  // general use.
  CastURLLoaderThrottleProvider(const CastURLLoaderThrottleProvider& other);

  THREAD_CHECKER(thread_checker_);

  DISALLOW_ASSIGN(CastURLLoaderThrottleProvider);
};

}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_CAST_URL_LOADER_THROTTLE_PROVIDER_H_
