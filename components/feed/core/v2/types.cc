// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/types.h"

#include "base/strings/string_number_conversions.h"

namespace feed {

std::string ToString(ContentRevision c) {
  return base::NumberToString(c.value());
}

ContentRevision ToContentRevision(const std::string& str) {
  uint32_t value;
  if (!base::StringToUint(str, &value))
    return {};
  return ContentRevision(value);
}

}  // namespace feed
