// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/favicon_types.h"

namespace favicon {

// IconMapping ----------------------------------------------------------------

IconMapping::IconMapping() = default;

IconMapping::IconMapping(const IconMapping&) = default;
IconMapping::IconMapping(IconMapping&&) noexcept = default;

IconMapping::~IconMapping() = default;

IconMapping& IconMapping::operator=(const IconMapping&) = default;

// IconMappingsForExpiry ------------------------------------------------------

IconMappingsForExpiry::IconMappingsForExpiry() = default;

IconMappingsForExpiry::IconMappingsForExpiry(
    const IconMappingsForExpiry& other) = default;

IconMappingsForExpiry::~IconMappingsForExpiry() = default;

// FaviconBitmap --------------------------------------------------------------

FaviconBitmap::FaviconBitmap() = default;

FaviconBitmap::FaviconBitmap(const FaviconBitmap& other) = default;

FaviconBitmap::~FaviconBitmap() = default;

// FaviconBitmapIDSize ---------------------------------------------------------

FaviconBitmapIDSize::FaviconBitmapIDSize() = default;

FaviconBitmapIDSize::~FaviconBitmapIDSize() = default;

}  // namespace favicon
