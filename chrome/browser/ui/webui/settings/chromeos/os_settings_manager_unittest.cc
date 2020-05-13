// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_manager.h"

#include "base/no_destructor.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_manager_factory.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_sections.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace settings {
namespace {

const std::vector<mojom::Section>& AllSections() {
  static const base::NoDestructor<std::vector<mojom::Section>> all_sections([] {
    int32_t min_section_value = static_cast<int32_t>(mojom::Section::kMinValue);
    int32_t max_section_value = static_cast<int32_t>(mojom::Section::kMaxValue);

    std::vector<mojom::Section> all_sections;
    for (int32_t i = min_section_value; i < max_section_value; ++i) {
      mojom::Section section = static_cast<mojom::Section>(i);

      // Not every value between the min and max values is a section, since some
      // sections have been deprecated.
      if (mojom::IsKnownEnumValue(section))
        all_sections.push_back(section);
    }

    return all_sections;
  }());

  return *all_sections;
}

}  // namespace

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

TEST_F(OsSettingsManagerTest, Sections) {
  // For each mojom::Section value, there should be an associated
  // OsSettingsSection class registered.
  for (const auto& section : AllSections()) {
    EXPECT_TRUE(manager_->sections_->GetSection(section))
        << "No OsSettingsSection instance created for " << section << ".";
  }
}

}  // namespace settings
}  // namespace chromeos
