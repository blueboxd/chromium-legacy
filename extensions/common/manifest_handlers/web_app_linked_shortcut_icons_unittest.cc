// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/web_app_linked_shortcut_icons.h"
#include "extensions/common/manifest_test.h"

namespace extensions {

namespace errors = manifest_errors;

using WebAppLinkedShortcutIconsHandlerTest = ManifestTest;

TEST_F(WebAppLinkedShortcutIconsHandlerTest, Valid) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("web_app_linked_shortcut_icons_valid.json",
                           extensions::Manifest::Location::INTERNAL,
                           extensions::Extension::FROM_BOOKMARK);
  ASSERT_TRUE(extension.get());
  ASSERT_TRUE(extension->from_bookmark());
}

TEST_F(WebAppLinkedShortcutIconsHandlerTest, InvalidNotFromBookmarkApp) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("web_app_linked_shortcut_icons_valid.json");

  ASSERT_TRUE(extension.get());
  ASSERT_FALSE(extension->from_bookmark());

  std::vector<InstallWarning> expected_install_warnings;
  expected_install_warnings.push_back(
      InstallWarning(errors::kInvalidWebAppLinkedShortcutIconsNotBookmarkApp));
  EXPECT_EQ(expected_install_warnings, extension->install_warnings());

  const WebAppLinkedShortcutIcons& web_app_linked_shortcut_icons =
      WebAppLinkedShortcutIcons::GetWebAppLinkedShortcutIcons(extension.get());
  ASSERT_TRUE(web_app_linked_shortcut_icons.shortcut_icon_infos.empty());
}

TEST_F(WebAppLinkedShortcutIconsHandlerTest, InvalidLinkedShortcutIndex) {
  LoadAndExpectError("web_app_linked_shortcut_icons_invalid1.json",
                     errors::kInvalidWebAppLinkedShortcutItemIndex,
                     extensions::Manifest::Location::INTERNAL,
                     extensions::Extension::FROM_BOOKMARK);
}

TEST_F(WebAppLinkedShortcutIconsHandlerTest, InvalidLinkedShortcutName) {
  LoadAndExpectError("web_app_linked_shortcut_icons_invalid2.json",
                     errors::kInvalidWebAppLinkedShortcutItemName,
                     extensions::Manifest::Location::INTERNAL,
                     extensions::Extension::FROM_BOOKMARK);
}

TEST_F(WebAppLinkedShortcutIconsHandlerTest, InvalidLinkedShortcutIconUrl) {
  LoadAndExpectError("web_app_linked_shortcut_icons_invalid3.json",
                     errors::kInvalidWebAppLinkedShortcutIconURL,
                     extensions::Manifest::Location::INTERNAL,
                     extensions::Extension::FROM_BOOKMARK);
}

TEST_F(WebAppLinkedShortcutIconsHandlerTest, InvalidLinkedShortcutIconSize) {
  LoadAndExpectError("web_app_linked_shortcut_icons_invalid4.json",
                     errors::kInvalidWebAppLinkedShortcutIconSize,
                     extensions::Manifest::Location::INTERNAL,
                     extensions::Extension::FROM_BOOKMARK);
}

TEST_F(WebAppLinkedShortcutIconsHandlerTest, InvalidLinkedShortcutIcons) {
  LoadAndExpectError("web_app_linked_shortcut_icons_invalid5.json",
                     errors::kInvalidWebAppLinkedShortcutIcons,
                     extensions::Manifest::Location::INTERNAL,
                     extensions::Extension::FROM_BOOKMARK);
}

TEST_F(WebAppLinkedShortcutIconsHandlerTest, InvalidLinkedShortcutIcon) {
  LoadAndExpectError("web_app_linked_shortcut_icons_invalid6.json",
                     errors::kInvalidWebAppLinkedShortcutIcon,
                     extensions::Manifest::Location::INTERNAL,
                     extensions::Extension::FROM_BOOKMARK);
}

}  // namespace extensions
