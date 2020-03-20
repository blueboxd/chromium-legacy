// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PROTO_UTIL_H_
#define COMPONENTS_FEED_CORE_V2_PROTO_UTIL_H_

#include <string>

#include "components/feed/core/proto/v2/wire/content_id.pb.h"

// Helper functions/classes for dealing with feed proto messages.

namespace feed {

std::string ContentIdString(const feedwire::ContentId&);
bool Equal(const feedwire::ContentId& a, const feedwire::ContentId& b);
bool CompareContentId(const feedwire::ContentId& a,
                      const feedwire::ContentId& b);

class ContentIdCompareFunctor {
 public:
  bool operator()(const feedwire::ContentId& a,
                  const feedwire::ContentId& b) const {
    return CompareContentId(a, b);
  }
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PROTO_UTIL_H_
