// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_FEED_STREAM_BACKGROUND_H_
#define COMPONENTS_FEED_CORE_V2_FEED_STREAM_BACKGROUND_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"

namespace feed {

// Counterpart of |FeedStream| which owns data that can be accessed in a
// background thread. Methods may only be called from a background thread.
// Use |FeedStream::RunInBackgroundAndReturn()| to access.
class FeedStreamBackground {
 public:
  FeedStreamBackground();
  ~FeedStreamBackground();
  FeedStreamBackground(const FeedStreamBackground&) = delete;
  FeedStreamBackground& operator=(const FeedStreamBackground&) = delete;

  base::WeakPtr<FeedStreamBackground> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // TODO(harringtond): Add FeedStore.

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<FeedStreamBackground> weak_ptr_factory_{this};
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_FEED_STREAM_BACKGROUND_H_
