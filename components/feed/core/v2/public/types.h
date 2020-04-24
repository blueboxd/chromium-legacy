// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_TYPES_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_TYPES_H_

#include "base/util/type_safety/id_type.h"
#include "base/version.h"
#include "components/version_info/channel.h"

namespace feed {

// Information about the Chrome build.
struct ChromeInfo {
  version_info::Channel channel{};
  base::Version version;
};

// A unique ID for an ephemeral change.
using EphemeralChangeId = util::IdTypeU32<class EphemeralChangeIdClass>;

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_TYPES_H_
