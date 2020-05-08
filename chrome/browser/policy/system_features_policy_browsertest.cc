// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/policy/system_features_disable_list_policy_handler.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"

namespace policy {

class SystemFeaturesPolicyTest : public PolicyTest {};

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest, DisableCameraBeforeInstall) {
  PolicyMap policies;
  std::unique_ptr<base::Value> system_features =
      std::make_unique<base::Value>(base::Value::Type::LIST);
  system_features->Append("camera");
  policies.Set(key::kSystemFeaturesDisableList, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::move(system_features), nullptr);
  UpdateProviderPolicy(policies);

  auto* profile = browser()->profile();
  extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
  extensions::ExtensionSystem::Get(profile)
      ->extension_service()
      ->component_loader()
      ->AddDefaultComponentExtensions(false);
  base::RunLoop().RunUntilIdle();

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  ASSERT_TRUE(
      registry->enabled_extensions().GetByID(extension_misc::kCameraAppId));

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  proxy->FlushMojoCallsForTesting();

  proxy->AppRegistryCache().ForOneApp(
      extension_misc::kCameraAppId, [](const apps::AppUpdate& update) {
        EXPECT_EQ(apps::mojom::Readiness::kDisabledByPolicy,
                  update.Readiness());
        EXPECT_TRUE(apps::IconEffects::kBlocked &
                    update.IconKey()->icon_effects);
      });

  policies.Set(key::kSystemFeaturesDisableList, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(), nullptr);
  UpdateProviderPolicy(policies);

  ASSERT_TRUE(
      registry->enabled_extensions().GetByID(extension_misc::kCameraAppId));

  proxy->FlushMojoCallsForTesting();
  proxy->AppRegistryCache().ForOneApp(
      extension_misc::kCameraAppId, [](const apps::AppUpdate& update) {
        EXPECT_EQ(apps::mojom::Readiness::kReady, update.Readiness());
        EXPECT_FALSE(apps::IconEffects::kBlocked &
                     update.IconKey()->icon_effects);
      });
}

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest, DisableCameraAfterInstall) {
  auto* profile = browser()->profile();
  extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
  extensions::ExtensionSystem::Get(profile)
      ->extension_service()
      ->component_loader()
      ->AddDefaultComponentExtensions(false);
  base::RunLoop().RunUntilIdle();

  PolicyMap policies;
  std::unique_ptr<base::Value> system_features =
      std::make_unique<base::Value>(base::Value::Type::LIST);
  system_features->Append("camera");
  policies.Set(key::kSystemFeaturesDisableList, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::move(system_features), nullptr);
  UpdateProviderPolicy(policies);

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  ASSERT_TRUE(
      registry->enabled_extensions().GetByID(extension_misc::kCameraAppId));

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  proxy->FlushMojoCallsForTesting();
  proxy->AppRegistryCache().ForOneApp(
      extension_misc::kCameraAppId, [](const apps::AppUpdate& update) {
        EXPECT_EQ(apps::mojom::Readiness::kDisabledByPolicy,
                  update.Readiness());
        EXPECT_TRUE(apps::IconEffects::kBlocked &
                    update.IconKey()->icon_effects);
      });

  policies.Set(key::kSystemFeaturesDisableList, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(), nullptr);
  UpdateProviderPolicy(policies);

  ASSERT_TRUE(
      registry->enabled_extensions().GetByID(extension_misc::kCameraAppId));

  proxy->FlushMojoCallsForTesting();
  proxy->AppRegistryCache().ForOneApp(
      extension_misc::kCameraAppId, [](const apps::AppUpdate& update) {
        EXPECT_EQ(apps::mojom::Readiness::kReady, update.Readiness());
        EXPECT_FALSE(apps::IconEffects::kBlocked &
                     update.IconKey()->icon_effects);
      });
}

}  // namespace policy
