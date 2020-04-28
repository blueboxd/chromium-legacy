// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/storable_conversion.h"

#include "base/check.h"

namespace content {

StorableConversion::StorableConversion(const std::string& conversion_data,
                                       const url::Origin& conversion_origin,
                                       const url::Origin& reporting_origin)
    : conversion_data_(conversion_data),
      conversion_origin_(conversion_origin),
      reporting_origin_(reporting_origin) {
  DCHECK(!reporting_origin_.opaque());
  DCHECK(!conversion_origin_.opaque());
}

StorableConversion::StorableConversion(const StorableConversion& other) =
    default;

StorableConversion::~StorableConversion() = default;

}  // namespace content
