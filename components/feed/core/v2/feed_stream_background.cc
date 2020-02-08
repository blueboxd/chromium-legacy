// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_stream_background.h"

namespace feed {

FeedStreamBackground::FeedStreamBackground() {
  // Allow construction on a different thread.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

FeedStreamBackground::~FeedStreamBackground() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace feed
