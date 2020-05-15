// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_WEB_APP_LINKED_SHORTCUT_ICONS_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_WEB_APP_LINKED_SHORTCUT_ICONS_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

class GURL;

namespace extensions {

// A structure to hold the parsed web app linked shortcut icon data.
struct WebAppLinkedShortcutIcons : public Extension::ManifestData {
  struct ShortcutIconInfo {
    ShortcutIconInfo();
    ~ShortcutIconInfo();

    int shortcut_item_index;
    base::string16 shortcut_item_name;
    GURL url;
    int size;
  };

  WebAppLinkedShortcutIcons();
  WebAppLinkedShortcutIcons(const WebAppLinkedShortcutIcons& other);
  ~WebAppLinkedShortcutIcons() override;

  static const WebAppLinkedShortcutIcons& GetWebAppLinkedShortcutIcons(
      const Extension* extension);

  std::vector<ShortcutIconInfo> shortcut_icon_infos;
};

// Parses the "web_app_linked_shortcut_icons" manifest key.
class WebAppLinkedShortcutIconsHandler : public ManifestHandler {
 public:
  WebAppLinkedShortcutIconsHandler();
  ~WebAppLinkedShortcutIconsHandler() override;
  WebAppLinkedShortcutIconsHandler(const WebAppLinkedShortcutIconsHandler&) =
      delete;
  WebAppLinkedShortcutIconsHandler& operator=(
      const WebAppLinkedShortcutIconsHandler&) = delete;

  // ManifestHandler:
  bool Parse(Extension* extension, base::string16* error) override;

 private:
  // ManifestHandler:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_WEB_APP_LINKED_SHORTCUT_ICONS_H_
