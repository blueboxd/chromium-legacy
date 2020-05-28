// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_manager.h"

#include "base/no_destructor.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/constants_util.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/hierarchy.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_manager_factory.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_sections.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace settings {

// Verifies the OsSettingsManager initialization flow. Behavioral functionality
// is tested via unit tests on the sub-elements owned by OsSettingsManager.
class OsSettingsManagerTest : public testing::Test {
 protected:
  OsSettingsManagerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~OsSettingsManagerTest() override = default;

  // testing::Test:
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    TestingProfile* profile =
        profile_manager_.CreateTestingProfile("TestingProfile");

    manager_ = OsSettingsManagerFactory::GetForProfile(profile);
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  OsSettingsManager* manager_;
};

TEST_F(OsSettingsManagerTest, Initialization) {
  // For each mojom::Section value, there should be an associated
  // OsSettingsSection class registered.
  for (const auto& section : constants::AllSections()) {
    EXPECT_TRUE(manager_->sections_->GetSection(section))
        << "No OsSettingsSection instance created for " << section << ".";
  }

  // Each mojom::Section should be registered in the hierarchy.
  for (const auto& section : constants::AllSections()) {
    const Hierarchy::SectionMetadata& metadata =
        manager_->hierarchy_->GetSectionMetadata(section);

    // Only the "About Chrome OS" section contains only a link to a subpage.
    EXPECT_EQ(metadata.only_contains_link_to_subpage,
              section == mojom::Section::kAboutChromeOs);
  }

  // Each mojom::Subpage should be registered in the hierarchy. Note that
  // GetSubpageMetadata() internally CHECK()s that the metadata exists before
  // returning it.
  for (const auto& subpage : constants::AllSubpages())
    manager_->hierarchy_->GetSubpageMetadata(subpage);

  // Each mojom::Setting should be registered in the hierarchy. Note that
  // GetSettingMetadata() internally CHECK()s that the metadata exists before
  // returning it.
  for (const auto& setting : constants::AllSettings())
    manager_->hierarchy_->GetSettingMetadata(setting);
}

}  // namespace settings
}  // namespace chromeos
