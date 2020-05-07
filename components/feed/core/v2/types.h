// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TYPES_H_
#define COMPONENTS_FEED_CORE_V2_TYPES_H_

#include <string>

#include "base/util/type_safety/id_type.h"
#include "components/feed/core/v2/public/types.h"

namespace feed {

// Make sure public types are included here too.
// See components/feed/core/v2/public/types.h.
using ::feed::ChromeInfo;
using ::feed::EphemeralChangeId;

// Uniquely identifies a revision of a |feedstore::Content|. If Content changes,
// it is assigned a new revision number.
using ContentRevision = util::IdTypeU32<class ContentRevisionClass>;

// ID for a stored pending action.
using LocalActionId = util::IdType32<class LocalActionIdClass>;

std::string ToString(ContentRevision c);
ContentRevision ToContentRevision(const std::string& str);

// Metadata sent with Feed requests.
struct RequestMetadata {
  ChromeInfo chrome_info;
  std::string language_tag;
  DisplayMetrics display_metrics;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_TYPES_H_
