// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/web_application_info.h"

WebApplicationIconInfo::WebApplicationIconInfo() : square_size_px(0) {}

WebApplicationIconInfo::WebApplicationIconInfo(const WebApplicationIconInfo&) =
    default;

WebApplicationIconInfo::WebApplicationIconInfo(WebApplicationIconInfo&&) =
    default;

WebApplicationIconInfo::~WebApplicationIconInfo() = default;

WebApplicationIconInfo& WebApplicationIconInfo::operator=(
    const WebApplicationIconInfo&) = default;

WebApplicationIconInfo& WebApplicationIconInfo::operator=(
    WebApplicationIconInfo&&) = default;

WebApplicationShortcutInfo::WebApplicationShortcutInfo() = default;

WebApplicationShortcutInfo::WebApplicationShortcutInfo(
    const WebApplicationShortcutInfo& other) = default;

WebApplicationShortcutInfo::WebApplicationShortcutInfo(
    WebApplicationShortcutInfo&&) = default;

WebApplicationShortcutInfo::~WebApplicationShortcutInfo() = default;

WebApplicationShortcutInfo& WebApplicationShortcutInfo::operator=(
    const WebApplicationShortcutInfo&) = default;

WebApplicationShortcutInfo& WebApplicationShortcutInfo::operator=(
    WebApplicationShortcutInfo&&) = default;

WebApplicationInfo::WebApplicationInfo()
    : mobile_capable(MOBILE_CAPABLE_UNSPECIFIED),
      generated_icon_color(SK_ColorTRANSPARENT),
      display_mode(blink::mojom::DisplayMode::kBrowser),
      open_as_window(false) {}

WebApplicationInfo::WebApplicationInfo(const WebApplicationInfo& other) =
    default;

WebApplicationInfo::~WebApplicationInfo() = default;

bool operator==(const WebApplicationIconInfo& icon_info1,
                const WebApplicationIconInfo& icon_info2) {
  return std::tie(icon_info1.url, icon_info1.square_size_px) ==
         std::tie(icon_info2.url, icon_info2.square_size_px);
}

std::ostream& operator<<(std::ostream& out,
                         const WebApplicationIconInfo& icon_info) {
  return out << "url: " << icon_info.url
             << " square_size_px: " << icon_info.square_size_px;
}
