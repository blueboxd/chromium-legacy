// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media_router/media_route.h"

#include <ostream>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/media_router/media_source.h"

namespace media_router {

// static
MediaRoute::Id MediaRoute::GetMediaRouteId(const std::string& presentation_id,
                                           const MediaSink::Id& sink_id,
                                           const MediaSource& source) {
  // TODO(https://crbug.com/816628): Can the route ID just be the presentation
  // id?
  return base::StringPrintf("urn:x-org.chromium:media:route:%s/%s/%s",
                            presentation_id.c_str(), sink_id.c_str(),
                            source.id().c_str());
}

MediaRoute::MediaRoute(const MediaRoute::Id& media_route_id,
                       const MediaSource& media_source,
                       const MediaSink::Id& media_sink_id,
                       const std::string& description,
                       bool is_local,
                       bool for_display)
    : media_route_id_(media_route_id),
      media_source_(media_source),
      media_sink_id_(media_sink_id),
      description_(description),
      is_local_(is_local),
      for_display_(for_display),
      is_incognito_(false),
      is_local_presentation_(false) {}

MediaRoute::MediaRoute(const MediaRoute& other) = default;

MediaRoute::MediaRoute() {}
MediaRoute::~MediaRoute() = default;

bool MediaRoute::operator==(const MediaRoute& other) const {
  return media_route_id_ == other.media_route_id_ &&
         presentation_id_ == other.presentation_id_ &&
         media_source_ == other.media_source_ &&
         media_sink_id_ == other.media_sink_id_ &&
         media_sink_name_ == other.media_sink_name_ &&
         description_ == other.description_ && is_local_ == other.is_local_ &&
         controller_type_ == other.controller_type_ &&
         for_display_ == other.for_display_ &&
         is_incognito_ == other.is_incognito_ &&
         is_local_presentation_ == other.is_local_presentation_;
}

std::ostream& operator<<(std::ostream& stream, const MediaRoute& route) {
  return stream << "MediaRoute{id=" << route.media_route_id_
                << ",source=" << route.media_source_.id()
                << ",sink=" << route.media_sink_id_ << "}";
}

}  // namespace media_router
