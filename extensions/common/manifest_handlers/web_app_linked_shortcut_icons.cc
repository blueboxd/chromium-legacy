// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/web_app_linked_shortcut_icons.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

namespace {

bool ParseShortcutIconValue(const base::Value& value,
                            WebAppLinkedShortcutIcons::ShortcutIconInfo* info,
                            base::string16* error) {
  const base::DictionaryValue* shortcut_icon_dict = nullptr;
  if (!value.GetAsDictionary(&shortcut_icon_dict)) {
    *error =
        base::UTF8ToUTF16(manifest_errors::kInvalidWebAppLinkedShortcutIcon);
    return false;
  }

  std::string url_string;
  if (!shortcut_icon_dict->GetString(keys::kWebAppLinkedShortcutIconURL,
                                     &url_string)) {
    *error =
        base::UTF8ToUTF16(manifest_errors::kInvalidWebAppLinkedShortcutIconURL);
    return false;
  }

  info->url = GURL(url_string);
  if (!info->url.is_valid()) {
    *error =
        base::UTF8ToUTF16(manifest_errors::kInvalidWebAppLinkedShortcutIconURL);
    return false;
  }

  if (!shortcut_icon_dict->GetInteger(keys::kWebAppLinkedShortcutIconSize,
                                      &info->size)) {
    *error = base::UTF8ToUTF16(
        manifest_errors::kInvalidWebAppLinkedShortcutIconSize);
    return false;
  }

  if (!shortcut_icon_dict->GetInteger(keys::kWebAppLinkedShortcutItemIndex,
                                      &info->shortcut_item_index)) {
    *error = base::UTF8ToUTF16(
        manifest_errors::kInvalidWebAppLinkedShortcutItemIndex);
    return false;
  }

  if (!shortcut_icon_dict->GetString(keys::kWebAppLinkedShortcutItemName,
                                     &info->shortcut_item_name)) {
    *error = base::UTF8ToUTF16(
        manifest_errors::kInvalidWebAppLinkedShortcutItemName);
    return false;
  }
  return true;
}

}  // namespace

WebAppLinkedShortcutIcons::ShortcutIconInfo::ShortcutIconInfo() = default;

WebAppLinkedShortcutIcons::ShortcutIconInfo::~ShortcutIconInfo() = default;

WebAppLinkedShortcutIcons::WebAppLinkedShortcutIcons() = default;

WebAppLinkedShortcutIcons::WebAppLinkedShortcutIcons(
    const WebAppLinkedShortcutIcons& other) = default;

WebAppLinkedShortcutIcons::~WebAppLinkedShortcutIcons() = default;

// static
const WebAppLinkedShortcutIcons&
WebAppLinkedShortcutIcons::GetWebAppLinkedShortcutIcons(
    const Extension* extension) {
  WebAppLinkedShortcutIcons* info = static_cast<WebAppLinkedShortcutIcons*>(
      extension->GetManifestData(keys::kWebAppLinkedShortcutIcons));
  if (info)
    return *info;

  static base::NoDestructor<WebAppLinkedShortcutIcons>
      empty_web_app_linked_shortcut_icons;
  return *empty_web_app_linked_shortcut_icons;
}

WebAppLinkedShortcutIconsHandler::WebAppLinkedShortcutIconsHandler() = default;

WebAppLinkedShortcutIconsHandler::~WebAppLinkedShortcutIconsHandler() = default;

bool WebAppLinkedShortcutIconsHandler::Parse(Extension* extension,
                                             base::string16* error) {
  // The "web_app_linked_shortcut_icons" key is only available for Bookmark
  // Apps. Including it elsewhere results in an install warning, and the linked
  // shortcut icons are not parsed.
  if (!extension->from_bookmark()) {
    extension->AddInstallWarning(InstallWarning(
        errors::kInvalidWebAppLinkedShortcutIconsNotBookmarkApp));
    return true;
  }

  auto web_app_linked_shortcut_icons =
      std::make_unique<WebAppLinkedShortcutIcons>();

  const base::Value* shortcut_icons_value = nullptr;
  if (!extension->manifest()->GetList(keys::kWebAppLinkedShortcutIcons,
                                      &shortcut_icons_value)) {
    *error =
        base::UTF8ToUTF16(manifest_errors::kInvalidWebAppLinkedShortcutIcons);
    return false;
  }

  base::Value::ConstListView shortcut_icons_list =
      shortcut_icons_value->GetList();
  web_app_linked_shortcut_icons->shortcut_icon_infos.reserve(
      shortcut_icons_list.size());
  for (const auto& shortcut_icon_value : shortcut_icons_list) {
    WebAppLinkedShortcutIcons::ShortcutIconInfo info;
    if (!ParseShortcutIconValue(shortcut_icon_value, &info, error)) {
      return false;
    }
    web_app_linked_shortcut_icons->shortcut_icon_infos.push_back(info);
  }
  extension->SetManifestData(keys::kWebAppLinkedShortcutIcons,
                             std::move(web_app_linked_shortcut_icons));
  return true;
}

base::span<const char* const> WebAppLinkedShortcutIconsHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kWebAppLinkedShortcutIcons};
  return kKeys;
}

}  // namespace extensions
