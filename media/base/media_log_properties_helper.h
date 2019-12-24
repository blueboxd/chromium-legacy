// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_LOG_PROPERTIES_HELPER_H_
#define MEDIA_BASE_MEDIA_LOG_PROPERTIES_HELPER_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "media/base/media_serializers.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// Forward declare the enum.
enum class MediaLogProperty;

// Allow only specific types for an individual property.
template <MediaLogProperty PROP, typename T>
struct MediaLogPropertyTypeSupport {};

// Lets us define the supported type in a single line in media_log_properties.h.
#define MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(PROPERTY, TYPE)                 \
  template <>                                                            \
  struct MediaLogPropertyTypeSupport<MediaLogProperty::PROPERTY, TYPE> { \
    static base::Value Convert(const TYPE& type) {                       \
      return MediaSerialize<TYPE>(type);                                 \
    }                                                                    \
  }

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_LOG_PROPERTIES_HELPER_H_
