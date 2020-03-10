// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_FEED_SERVICE_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_FEED_SERVICE_H_

#include <memory>
#include "components/feed/core/v2/public/feed_stream_api.h"
#include "components/keyed_service/core/keyed_service.h"

namespace feed {

class FeedService : public KeyedService {
 public:
  explicit FeedService(std::unique_ptr<FeedStreamApi> stream);
  ~FeedService() override;

  FeedStreamApi* GetStream() { return stream_.get(); }

 private:
  std::unique_ptr<FeedStreamApi> stream_;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_FEED_SERVICE_H_
