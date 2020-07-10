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

sync_pb::WebAppSpecifics WebAppToSyncProto(const WebApp& app) {
  sync_pb::WebAppSpecifics sync_proto;
  sync_proto.set_launch_url(app.launch_url().spec());
  sync_proto.set_user_display_mode(
      ToWebAppSpecificsUserDisplayMode(app.user_display_mode()));
  sync_proto.set_name(app.sync_fallback_data().name);
  if (app.sync_fallback_data().theme_color.has_value())
    sync_proto.set_theme_color(app.sync_fallback_data().theme_color.value());
  if (app.user_page_ordinal().IsValid()) {
    sync_proto.set_user_page_ordinal(app.user_page_ordinal().ToInternalValue());
  }
  if (app.user_launch_ordinal().IsValid()) {
    sync_proto.set_user_launch_ordinal(
        app.user_launch_ordinal().ToInternalValue());
  }
  if (app.sync_fallback_data().scope.is_valid())
    sync_proto.set_scope(app.sync_fallback_data().scope.spec());
  for (const WebApplicationIconInfo& icon_info :
       app.sync_fallback_data().icon_infos) {
    sync_pb::WebAppIconInfo* icon_info_proto = sync_proto.add_icon_infos();
    icon_info_proto->set_url(icon_info.url.spec());
    if (icon_info.square_size_px.has_value())
      icon_info_proto->set_size_in_px(icon_info.square_size_px.value());
  }
  return sync_proto;
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

::sync_pb::WebAppSpecifics::UserDisplayMode ToWebAppSpecificsUserDisplayMode(
    DisplayMode user_display_mode) {
  switch (user_display_mode) {
    case DisplayMode::kBrowser:
      return ::sync_pb::WebAppSpecifics::BROWSER;
    case DisplayMode::kUndefined:
    case DisplayMode::kMinimalUi:
    case DisplayMode::kFullscreen:
      NOTREACHED();
      FALLTHROUGH;
    case DisplayMode::kStandalone:
      return ::sync_pb::WebAppSpecifics::STANDALONE;
  }
}

}  // namespace web_app
