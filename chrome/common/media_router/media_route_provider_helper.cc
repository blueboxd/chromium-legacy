// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media_router/media_route_provider_helper.h"

#include "base/notreached.h"
#include "base/strings/string_piece.h"

constexpr const char kExtension[] = "EXTENSION";
constexpr const char kWiredDisplay[] = "WIRED_DISPLAY";
constexpr const char kDial[] = "DIAL";
constexpr const char kCast[] = "CAST";
constexpr const char kUnknown[] = "UNKNOWN";

namespace media_router {

const char* ProviderIdToString(MediaRouteProviderId provider_id) {
  switch (provider_id) {
    case EXTENSION:
      return kExtension;
    case WIRED_DISPLAY:
      return kWiredDisplay;
    case CAST:
      return kCast;
    case DIAL:
      return kDial;
    case UNKNOWN:
      return kUnknown;
  }

  NOTREACHED() << "Unknown provider_id " << static_cast<int>(provider_id);
  return "Unknown provider_id";
}

MediaRouteProviderId ProviderIdFromString(base::StringPiece provider_id) {
  if (provider_id == kExtension) {
    return MediaRouteProviderId::EXTENSION;
  } else if (provider_id == kWiredDisplay) {
    return MediaRouteProviderId::WIRED_DISPLAY;
  } else if (provider_id == kCast) {
    return MediaRouteProviderId::CAST;
  } else if (provider_id == kDial) {
    return MediaRouteProviderId::DIAL;
  } else {
    return MediaRouteProviderId::UNKNOWN;
  }
}

}  // namespace media_router
