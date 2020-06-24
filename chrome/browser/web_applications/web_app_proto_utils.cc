// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_proto_utils.h"

namespace web_app {

base::Optional<std::vector<WebApplicationIconInfo>> ParseWebAppIconInfos(
    const char* container_name_for_logging,
    RepeatedIconInfosProto icon_infos_proto) {
  std::vector<WebApplicationIconInfo> icon_infos;
  for (const sync_pb::WebAppIconInfo& icon_info_proto : icon_infos_proto) {
    WebApplicationIconInfo icon_info;

    if (icon_info_proto.has_size_in_px())
      icon_info.square_size_px = icon_info_proto.size_in_px();

    if (!icon_info_proto.has_url()) {
      DLOG(ERROR) << container_name_for_logging << " IconInfo has missing url";
      return base::nullopt;
    }
    icon_info.url = GURL(icon_info_proto.url());
    if (icon_info.url.is_empty() || !icon_info.url.is_valid()) {
      DLOG(ERROR) << container_name_for_logging << " IconInfo has invalid url: "
                  << icon_info.url.possibly_invalid_spec();
      return base::nullopt;
    }

    icon_infos.push_back(std::move(icon_info));
  }
  return icon_infos;
}

base::Optional<WebApp::SyncFallbackData> ParseSyncFallbackDataStruct(
    const sync_pb::WebAppSpecifics& sync_proto) {
  WebApp::SyncFallbackData parsed_sync_fallback_data;

  parsed_sync_fallback_data.name = sync_proto.name();

  if (sync_proto.has_theme_color())
    parsed_sync_fallback_data.theme_color = sync_proto.theme_color();

  if (sync_proto.has_scope()) {
    parsed_sync_fallback_data.scope = GURL(sync_proto.scope());
    if (!parsed_sync_fallback_data.scope.is_valid()) {
      DLOG(ERROR) << "WebAppSpecifics scope has invalid url: "
                  << parsed_sync_fallback_data.scope.possibly_invalid_spec();
      return base::nullopt;
    }
  }

  base::Optional<std::vector<WebApplicationIconInfo>> parsed_icon_infos =
      ParseWebAppIconInfos("WebAppSpecifics", sync_proto.icon_infos());
  if (!parsed_icon_infos.has_value())
    return base::nullopt;

  parsed_sync_fallback_data.icon_infos = std::move(parsed_icon_infos.value());

  return parsed_sync_fallback_data;
}

}  // namespace web_app
