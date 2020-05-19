// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/settings_overridden_params_providers.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"

class SettingsOverriddenParamsProvidersBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  // Installs a new extension that controls the default search engine.
  const extensions::Extension* AddExtensionControllingSearch() {
    const extensions::Extension* extension =
        InstallExtensionWithPermissionsGranted(
            test_data_dir_.AppendASCII("search_provider_override"), 1);
    EXPECT_EQ(extension,
              extensions::GetExtensionOverridingSearchEngine(profile()));
    return extension;
  }
};

// The chrome_settings_overrides API that allows extensions to override the
// default search provider is only available on Windows and Mac.
#if defined(OS_WIN) || defined(OS_MACOSX)

// NOTE: It's very unfortunate that this has to be a browsertest. Unfortunately,
// a few bits here - the TemplateURLService in particular - don't play nicely
// with a unittest environment.
IN_PROC_BROWSER_TEST_F(SettingsOverriddenParamsProvidersBrowserTest,
                       GetExtensionControllingSearch) {
  // With no extensions installed, there should be no controlling extension.
  EXPECT_EQ(base::nullopt,
            settings_overridden_params::GetSearchOverriddenParams(profile()));

  // Install an extension, but not one that overrides the default search engine.
  // There should still be no controlling extension.
  InstallExtensionWithPermissionsGranted(
      test_data_dir_.AppendASCII("simple_with_icon"), 1);
  EXPECT_EQ(base::nullopt,
            settings_overridden_params::GetSearchOverriddenParams(profile()));

  // Finally, install an extension that overrides the default search engine.
  // It should be the controlling extension.
  const extensions::Extension* search_extension =
      AddExtensionControllingSearch();
  base::Optional<ExtensionSettingsOverriddenDialog::Params> params =
      settings_overridden_params::GetSearchOverriddenParams(profile());
  ASSERT_TRUE(params);
  EXPECT_EQ(search_extension->id(), params->controlling_extension_id);

  // Validate the body message, since it has a bit of formatting applied.
  EXPECT_EQ(
      "The Search Override Extension extension changed search to use "
      "example.com",
      base::UTF16ToUTF8(params->dialog_message));
}

#endif  // defined(OS_WIN) || defined(OS_MACOSX)
