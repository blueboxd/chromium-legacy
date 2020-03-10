// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/feed_service.h"

namespace feed {

FeedService::FeedService(std::unique_ptr<FeedStreamApi> stream)
    : stream_(std::move(stream)) {}
FeedService::~FeedService() = default;

}  // namespace feed
