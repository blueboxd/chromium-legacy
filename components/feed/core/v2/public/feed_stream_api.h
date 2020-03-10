// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_FEED_STREAM_API_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_FEED_STREAM_API_H_

namespace feed {

// This is the public access point for interacting with the Feed stream
// contents.
class FeedStreamApi {
 public:
  FeedStreamApi() = default;
  virtual ~FeedStreamApi() = default;

  virtual void SetArticlesListVisible(bool is_visible) = 0;
  virtual bool IsArticlesListVisible() = 0;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_FEED_STREAM_API_H_
