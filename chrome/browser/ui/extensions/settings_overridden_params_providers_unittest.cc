// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/settings_overridden_params_providers.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/extensions/extension_web_ui_override_registrar.h"
#include "chrome/common/webui_url_constants.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"

class SettingsOverriddenParamsProvidersUnitTest
    : public extensions::ExtensionServiceTestBase {
 public:
  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();

    // The NtpOverriddenDialogController rellies on ExtensionWebUI; ensure one
    // exists.
    extensions::ExtensionWebUIOverrideRegistrar::GetFactoryInstance()
        ->SetTestingFactoryAndUse(
            profile(), base::Bind([](content::BrowserContext* context)
                                      -> std::unique_ptr<KeyedService> {
              return std::make_unique<
                  extensions::ExtensionWebUIOverrideRegistrar>(context);
            }));
  }

  // Adds a new extension that overrides the NTP.
  const extensions::Extension* AddExtensionControllingNewTab() {
    std::unique_ptr<base::Value> chrome_url_overrides =
        extensions::DictionaryBuilder().Set("newtab", "newtab.html").Build();
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder("ntp override")
            .SetLocation(extensions::Manifest::INTERNAL)
            .SetManifestKey("chrome_url_overrides",
                            std::move(chrome_url_overrides))
            .Build();

    service()->AddExtension(extension.get());
    EXPECT_EQ(extension, ExtensionWebUI::GetExtensionControllingURL(
                             GURL(chrome::kChromeUINewTabURL), profile()));

    return extension.get();
  }
};

TEST_F(SettingsOverriddenParamsProvidersUnitTest,
       GetExtensionControllingNewTab) {
  // With no extensions installed, there should be no controlling extension.
  EXPECT_EQ(base::nullopt,
            settings_overridden_params::GetNtpOverriddenParams(profile()));

  // Install an extension, but not one that overrides the NTP. There should
  // still be no controlling extension.
  scoped_refptr<const extensions::Extension> regular_extension =
      extensions::ExtensionBuilder("regular").Build();
  service()->AddExtension(regular_extension.get());
  EXPECT_EQ(base::nullopt,
            settings_overridden_params::GetNtpOverriddenParams(profile()));

  // Finally, install an extension that overrides the NTP. It should be the
  // controlling extension.
  const extensions::Extension* ntp_extension = AddExtensionControllingNewTab();
  base::Optional<ExtensionSettingsOverriddenDialog::Params> params =
      settings_overridden_params::GetNtpOverriddenParams(profile());
  ASSERT_TRUE(params);
  EXPECT_EQ(ntp_extension->id(), params->controlling_extension_id);
}
