// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_service.h"

#include <vector>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/search/background/ntp_background_service.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/search/instant_unittest_base.h"
#include "chrome/browser/search/ntp_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/url_constants.h"
#include "components/ntp_tiles/features.h"
#include "components/ntp_tiles/ntp_tile.h"
#include "components/ntp_tiles/section_type.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/native_theme/test_native_theme.h"
#include "url/gurl.h"

namespace {

class MockInstantServiceObserver : public InstantServiceObserver {
 public:
  MOCK_METHOD1(ThemeInfoChanged, void(const ThemeBackgroundInfo&));
  MOCK_METHOD1(MostVisitedInfoChanged, void(const InstantMostVisitedInfo&));
};

base::DictionaryValue GetBackgroundInfoAsDict(const GURL& background_url) {
  base::DictionaryValue background_info;
  background_info.SetKey("background_url", base::Value(background_url.spec()));
  background_info.SetKey("attribution_line_1", base::Value(std::string()));
  background_info.SetKey("attribution_line_2", base::Value(std::string()));
  background_info.SetKey("attribution_action_url", base::Value(std::string()));

  return background_info;
}

class MockInstantService : public InstantService {
 public:
  explicit MockInstantService(Profile* profile) : InstantService(profile) {}
  ~MockInstantService() override = default;

  MOCK_METHOD0(ResetCustomLinks, bool());
  MOCK_METHOD0(ResetCustomBackgroundThemeInfo, void());
};

bool CheckBackgroundColor(SkColor color,
                          const base::DictionaryValue* background_info) {
  if (!background_info)
    return false;

  const base::Value* background_color =
      background_info->FindKey(kNtpCustomBackgroundMainColor);
  if (!background_color)
    return false;

  return color == static_cast<uint32_t>(background_color->GetInt());
}
}  // namespace

using InstantServiceTest = InstantUnitTestBase;

TEST_F(InstantServiceTest, GetNTPTileSuggestion) {
  ntp_tiles::NTPTile some_tile;
  some_tile.source = ntp_tiles::TileSource::TOP_SITES;
  some_tile.title_source = ntp_tiles::TileTitleSource::TITLE_TAG;
  ntp_tiles::NTPTilesVector suggestions{some_tile};

  std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector> suggestions_map;
  suggestions_map[ntp_tiles::SectionType::PERSONALIZED] = suggestions;

  instant_service_->OnURLsAvailable(suggestions_map);

  auto items = instant_service_->most_visited_info_->items;
  ASSERT_EQ(1, (int)items.size());
  EXPECT_EQ(ntp_tiles::TileSource::TOP_SITES, items[0].source);
  EXPECT_EQ(ntp_tiles::TileTitleSource::TITLE_TAG, items[0].title_source);
}

TEST_F(InstantServiceTest, DeleteThumbnailDataIfExists) {
  const std::string kTestData("test");
  base::FilePath database_dir =
      profile()->GetPath().Append(FILE_PATH_LITERAL("Thumbnails"));

  if (!base::PathExists(database_dir))
    ASSERT_TRUE(base::CreateDirectory(database_dir));
  ASSERT_NE(-1, base::WriteFile(
                    database_dir.Append(FILE_PATH_LITERAL("test_thumbnail")),
                    kTestData.c_str(), kTestData.length()));

  // Delete the thumbnail directory.
  base::MockCallback<base::OnceCallback<void(bool)>> result;
  EXPECT_CALL(result, Run(true));
  instant_service_->DeleteThumbnailDataIfExists(
      profile()->GetPath(),
      base::Optional<base::OnceCallback<void(bool)>>(result.Get()));
  thread_bundle()->RunUntilIdle();
  EXPECT_FALSE(base::PathExists(database_dir));

  // Delete should fail since the path does not exist.
  base::MockCallback<base::OnceCallback<void(bool)>> result2;
  EXPECT_CALL(result2, Run(false));
  instant_service_->DeleteThumbnailDataIfExists(
      profile()->GetPath(),
      base::Optional<base::OnceCallback<void(bool)>>(result2.Get()));
  thread_bundle()->RunUntilIdle();
  EXPECT_FALSE(base::PathExists(database_dir));
}

TEST_F(InstantServiceTest, DoesToggleMostVisitedOrCustomLinks) {
  sync_preferences::TestingPrefServiceSyncable* pref_service =
      profile()->GetTestingPrefService();
  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  ASSERT_FALSE(pref_service->GetBoolean(prefs::kNtpUseMostVisitedTiles));
  ASSERT_FALSE(instant_service_->most_visited_info_->use_most_visited);

  // Enable most visited tiles.
  EXPECT_TRUE(instant_service_->ToggleMostVisitedOrCustomLinks());
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kNtpUseMostVisitedTiles));
  EXPECT_TRUE(instant_service_->most_visited_info_->use_most_visited);

  // Disable most visited tiles.
  EXPECT_TRUE(instant_service_->ToggleMostVisitedOrCustomLinks());
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kNtpUseMostVisitedTiles));
  EXPECT_FALSE(instant_service_->most_visited_info_->use_most_visited);

  // Should do nothing if this is a non-Google NTP.
  SetUserSelectedDefaultSearchProvider("https://www.search.com");
  EXPECT_FALSE(instant_service_->ToggleMostVisitedOrCustomLinks());
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kNtpUseMostVisitedTiles));
  EXPECT_FALSE(instant_service_->most_visited_info_->use_most_visited);
}

TEST_F(InstantServiceTest, DoesToggleShortcutsVisibility) {
  sync_preferences::TestingPrefServiceSyncable* pref_service =
      profile()->GetTestingPrefService();
  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  ASSERT_TRUE(pref_service->GetBoolean(prefs::kNtpShortcutsVisible));
  ASSERT_TRUE(instant_service_->most_visited_info_->is_visible);

  // Hide shortcuts.
  EXPECT_TRUE(instant_service_->ToggleShortcutsVisibility());
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kNtpShortcutsVisible));
  EXPECT_FALSE(instant_service_->most_visited_info_->is_visible);

  // Show shortcuts.
  EXPECT_TRUE(instant_service_->ToggleShortcutsVisibility());
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kNtpShortcutsVisible));
  EXPECT_TRUE(instant_service_->most_visited_info_->is_visible);

  // Should do nothing if this is a non-Google NTP.
  SetUserSelectedDefaultSearchProvider("https://www.search.com");
  EXPECT_FALSE(instant_service_->ToggleShortcutsVisibility());
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kNtpShortcutsVisible));
  EXPECT_TRUE(instant_service_->most_visited_info_->is_visible);
}

TEST_F(InstantServiceTest,
       DisableUndoCustomLinkActionForNonGoogleSearchProvider) {
  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  EXPECT_TRUE(instant_service_->UndoCustomLinkAction());

  SetUserSelectedDefaultSearchProvider("https://www.search.com");
  EXPECT_FALSE(instant_service_->UndoCustomLinkAction());
}

TEST_F(InstantServiceTest, DisableResetCustomLinksForNonGoogleSearchProvider) {
  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  EXPECT_TRUE(instant_service_->ResetCustomLinks());

  SetUserSelectedDefaultSearchProvider("https://www.search.com");
  EXPECT_FALSE(instant_service_->ResetCustomLinks());
}

TEST_F(InstantServiceTest, IsCustomLinksEnabled) {
  sync_preferences::TestingPrefServiceSyncable* pref_service =
      profile()->GetTestingPrefService();

  // Test that custom links are only enabled when Most Visited is toggled off
  // and this is a Google NTP.
  pref_service->SetBoolean(prefs::kNtpUseMostVisitedTiles, false);
  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  EXPECT_TRUE(instant_service_->IsCustomLinksEnabled());

  // All other cases should return false.
  SetUserSelectedDefaultSearchProvider("https://www.search.com");
  EXPECT_FALSE(instant_service_->IsCustomLinksEnabled());
  pref_service->SetBoolean(prefs::kNtpUseMostVisitedTiles, true);
  EXPECT_FALSE(instant_service_->IsCustomLinksEnabled());
  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  EXPECT_FALSE(instant_service_->IsCustomLinksEnabled());
}

TEST_F(InstantServiceTest, SetCustomBackgroundURL) {
  ASSERT_FALSE(instant_service_->IsCustomBackgroundSet());
  const GURL kUrl("https://www.foo.com");

  instant_service_->AddValidBackdropUrlForTesting(kUrl);
  instant_service_->SetCustomBackgroundURLWithAttributions(kUrl, std::string(), std::string(), GURL());

  ThemeBackgroundInfo* theme_info = instant_service_->GetInitializedThemeInfo();
  EXPECT_EQ(kUrl, theme_info->custom_background_url);
  EXPECT_TRUE(instant_service_->IsCustomBackgroundSet());
}

TEST_F(InstantServiceTest, SetCustomBackgroundURLInvalidURL) {
  ASSERT_FALSE(instant_service_->IsCustomBackgroundSet());
  const GURL kInvalidUrl("foo");
  const GURL kValidUrl("https://www.foo.com");
  instant_service_->AddValidBackdropUrlForTesting(kValidUrl);
  instant_service_->SetCustomBackgroundURL(kValidUrl);

  ThemeBackgroundInfo* theme_info = instant_service_->GetInitializedThemeInfo();
  EXPECT_EQ(kValidUrl.spec(), theme_info->custom_background_url.spec());

  instant_service_->SetCustomBackgroundURLWithAttributions(kInvalidUrl, std::string(), std::string(), GURL());

  theme_info = instant_service_->GetInitializedThemeInfo();
  EXPECT_EQ(std::string(), theme_info->custom_background_url.spec());
  EXPECT_FALSE(instant_service_->IsCustomBackgroundSet());
}

TEST_F(InstantServiceTest, SetCustomBackgroundURLWithAttributions) {
  ASSERT_FALSE(instant_service_->IsCustomBackgroundSet());
  const GURL kUrl("https://www.foo.com");
  const std::string kAttributionLine1 = "foo";
  const std::string kAttributionLine2 = "bar";
  const GURL kActionUrl("https://www.bar.com");
  instant_service_->AddValidBackdropUrlForTesting(kUrl);
  instant_service_->SetCustomBackgroundURLWithAttributions(
      kUrl, kAttributionLine1, kAttributionLine2, kActionUrl);

  ThemeBackgroundInfo* theme_info = instant_service_->GetInitializedThemeInfo();
  EXPECT_EQ(kUrl, theme_info->custom_background_url);
  EXPECT_EQ(kAttributionLine1,
            theme_info->custom_background_attribution_line_1);
  EXPECT_EQ(kAttributionLine2,
            theme_info->custom_background_attribution_line_2);
  EXPECT_EQ(kActionUrl, theme_info->custom_background_attribution_action_url);
  EXPECT_TRUE(instant_service_->IsCustomBackgroundSet());
}

TEST_F(InstantServiceTest, ChangingSearchProviderClearsThemeInfoAndPref) {
  ASSERT_FALSE(instant_service_->IsCustomBackgroundSet());
  const GURL kUrl("https://www.foo.com");
  const std::string kAttributionLine1 = "foo";
  const std::string kAttributionLine2 = "bar";
  const GURL kActionUrl("https://www.bar.com");

  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  instant_service_->AddValidBackdropUrlForTesting(kUrl);
  instant_service_->SetCustomBackgroundURLWithAttributions(
      kUrl, kAttributionLine1, kAttributionLine2, kActionUrl);

  ThemeBackgroundInfo* theme_info = instant_service_->GetInitializedThemeInfo();
  EXPECT_EQ(kUrl, theme_info->custom_background_url);
  EXPECT_EQ(kAttributionLine1,
            theme_info->custom_background_attribution_line_1);
  EXPECT_EQ(kAttributionLine2,
            theme_info->custom_background_attribution_line_2);
  EXPECT_EQ(kActionUrl, theme_info->custom_background_attribution_action_url);
  EXPECT_TRUE(instant_service_->IsCustomBackgroundSet());

  SetUserSelectedDefaultSearchProvider("https://www.search.com");
  instant_service_->UpdateThemeInfo();

  theme_info = instant_service_->GetInitializedThemeInfo();
  EXPECT_EQ(GURL(), theme_info->custom_background_url);
  EXPECT_EQ(std::string(), theme_info->custom_background_attribution_line_1);
  EXPECT_EQ(std::string(), theme_info->custom_background_attribution_line_2);
  EXPECT_EQ(GURL(), theme_info->custom_background_attribution_action_url);
  EXPECT_FALSE(instant_service_->IsCustomBackgroundSet());

  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  instant_service_->UpdateThemeInfo();

  theme_info = instant_service_->GetInitializedThemeInfo();
  EXPECT_EQ(GURL(), theme_info->custom_background_url);
  EXPECT_EQ(std::string(), theme_info->custom_background_attribution_line_1);
  EXPECT_EQ(std::string(), theme_info->custom_background_attribution_line_2);
  EXPECT_EQ(GURL(), theme_info->custom_background_attribution_action_url);
  EXPECT_FALSE(instant_service_->IsCustomBackgroundSet());
}

TEST_F(InstantServiceTest, LocalBackgroundImageCopyCreated) {
  ASSERT_FALSE(instant_service_->IsCustomBackgroundSet());
  const GURL kUrl("chrome-search://local-ntp/background.jpg");

  base::FilePath profile_path = profile()->GetPath();
  base::FilePath path(profile_path.AppendASCII("test_file"));
  base::FilePath copy_path(profile_path.AppendASCII(
      chrome::kChromeSearchLocalNtpBackgroundFilename));

  base::WriteFile(path, "background_image", 16);

  instant_service_->SelectLocalBackgroundImage(path);

  thread_bundle()->RunUntilIdle();

  bool file_exists = base::PathExists(copy_path);

  EXPECT_EQ(true, file_exists);
  EXPECT_EQ(true, profile()->GetTestingPrefService()->GetBoolean(
                      prefs::kNtpCustomBackgroundLocalToDevice));
  EXPECT_TRUE(instant_service_->IsCustomBackgroundSet());
}

TEST_F(InstantServiceTest,
       ChangingSearchProviderRemovesLocalBackgroundImageCopy) {
  ASSERT_FALSE(instant_service_->IsCustomBackgroundSet());
  base::FilePath profile_path = profile()->GetPath();
  base::FilePath path(profile_path.AppendASCII(
      chrome::kChromeSearchLocalNtpBackgroundFilename));

  base::WriteFile(path, "background_image", 16);

  SetUserSelectedDefaultSearchProvider("https://www.search.com");
  instant_service_->UpdateThemeInfo();

  thread_bundle()->RunUntilIdle();

  bool file_exists = base::PathExists(path);

  EXPECT_EQ(false, file_exists);
  EXPECT_EQ(false, profile()->GetTestingPrefService()->GetBoolean(
                       prefs::kNtpCustomBackgroundLocalToDevice));
  EXPECT_FALSE(instant_service_->IsCustomBackgroundSet());
}

TEST_F(InstantServiceTest, SettingUrlRemovesLocalBackgroundImageCopy) {
  ASSERT_FALSE(instant_service_->IsCustomBackgroundSet());
  const GURL kUrl("https://www.foo.com");

  base::FilePath profile_path = profile()->GetPath();
  base::FilePath path(profile_path.AppendASCII(
      chrome::kChromeSearchLocalNtpBackgroundFilename));

  base::WriteFile(path, "background_image", 16);

  instant_service_->AddValidBackdropUrlForTesting(kUrl);
  instant_service_->SetCustomBackgroundURL(kUrl);
  instant_service_->UpdateThemeInfo();

  thread_bundle()->RunUntilIdle();

  bool file_exists = base::PathExists(path);

  EXPECT_EQ(false, file_exists);
  EXPECT_EQ(false, profile()->GetTestingPrefService()->GetBoolean(
                       prefs::kNtpCustomBackgroundLocalToDevice));
  ASSERT_TRUE(instant_service_->IsCustomBackgroundSet());
}

TEST_F(InstantServiceTest, CustomBackgroundAttributionActionUrlReset) {
  ASSERT_FALSE(instant_service_->IsCustomBackgroundSet());
  const GURL kUrl("https://www.foo.com");
  const std::string kAttributionLine1 = "foo";
  const std::string kAttributionLine2 = "bar";
  const GURL kHttpsActionUrl("https://www.bar.com");
  const GURL kHttpActionUrl("http://www.bar.com");

  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  instant_service_->AddValidBackdropUrlForTesting(kUrl);
  instant_service_->SetCustomBackgroundURLWithAttributions(
      kUrl, kAttributionLine1, kAttributionLine2, kHttpsActionUrl);

  ThemeBackgroundInfo* theme_info = instant_service_->GetInitializedThemeInfo();
  EXPECT_EQ(kHttpsActionUrl,
            theme_info->custom_background_attribution_action_url);
  EXPECT_TRUE(instant_service_->IsCustomBackgroundSet());

  instant_service_->SetCustomBackgroundURLWithAttributions(
      kUrl, kAttributionLine1, kAttributionLine2, kHttpActionUrl);

  theme_info = instant_service_->GetInitializedThemeInfo();
  EXPECT_EQ(GURL(), theme_info->custom_background_attribution_action_url);
  EXPECT_TRUE(instant_service_->IsCustomBackgroundSet());

  instant_service_->SetCustomBackgroundURLWithAttributions(
      kUrl, kAttributionLine1, kAttributionLine2, kHttpsActionUrl);

  theme_info = instant_service_->GetInitializedThemeInfo();
  EXPECT_EQ(kHttpsActionUrl,
            theme_info->custom_background_attribution_action_url);
  EXPECT_TRUE(instant_service_->IsCustomBackgroundSet());

  instant_service_->SetCustomBackgroundURLWithAttributions(
      kUrl, kAttributionLine1, kAttributionLine2, GURL());

  theme_info = instant_service_->GetInitializedThemeInfo();
  EXPECT_EQ(GURL(), theme_info->custom_background_attribution_action_url);
  EXPECT_TRUE(instant_service_->IsCustomBackgroundSet());
}

TEST_F(InstantServiceTest, UpdatingPrefUpdatesThemeInfo) {
  ASSERT_FALSE(instant_service_->IsCustomBackgroundSet());
  const GURL kUrlFoo("https://www.foo.com");
  const GURL kUrlBar("https://www.bar.com");

  sync_preferences::TestingPrefServiceSyncable* pref_service =
      profile()->GetTestingPrefService();
  pref_service->SetUserPref(
      prefs::kNtpCustomBackgroundDict,
      std::make_unique<base::Value>(GetBackgroundInfoAsDict(kUrlFoo)));

  ThemeBackgroundInfo* theme_info = instant_service_->GetInitializedThemeInfo();
  EXPECT_EQ(kUrlFoo, theme_info->custom_background_url);
  EXPECT_TRUE(instant_service_->IsCustomBackgroundSet());

  pref_service->SetUserPref(
      prefs::kNtpCustomBackgroundDict,
      std::make_unique<base::Value>(GetBackgroundInfoAsDict(kUrlBar)));

  theme_info = instant_service_->GetInitializedThemeInfo();
  EXPECT_EQ(kUrlBar, theme_info->custom_background_url);
  EXPECT_EQ(false,
            pref_service->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice));
  EXPECT_TRUE(instant_service_->IsCustomBackgroundSet());
}

TEST_F(InstantServiceTest, SetLocalImage) {
  ASSERT_FALSE(instant_service_->IsCustomBackgroundSet());
  const GURL kUrl("chrome-search://local-ntp/background.jpg?123456789");

  sync_preferences::TestingPrefServiceSyncable* pref_service =
      profile()->GetTestingPrefService();

  base::FilePath profile_path = profile()->GetPath();
  base::FilePath path(profile_path.AppendASCII(
      chrome::kChromeSearchLocalNtpBackgroundFilename));
  base::WriteFile(path, "background_image", 16);
  base::ThreadPoolInstance::Get()->FlushForTesting();

  instant_service_->SelectLocalBackgroundImage(path);
  thread_bundle()->RunUntilIdle();

  ThemeBackgroundInfo* theme_info = instant_service_->GetInitializedThemeInfo();
  EXPECT_TRUE(base::StartsWith(theme_info->custom_background_url.spec(),
                               chrome::kChromeSearchLocalNtpBackgroundUrl,
                               base::CompareCase::SENSITIVE));
  EXPECT_TRUE(
      pref_service->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice));
  EXPECT_TRUE(instant_service_->IsCustomBackgroundSet());
}

TEST_F(InstantServiceTest, SyncPrefOverridesLocalImage) {
  ASSERT_FALSE(instant_service_->IsCustomBackgroundSet());
  const GURL kUrl("https://www.foo.com/");

  sync_preferences::TestingPrefServiceSyncable* pref_service =
      profile()->GetTestingPrefService();

  base::FilePath profile_path = profile()->GetPath();
  base::FilePath path(profile_path.AppendASCII(
      chrome::kChromeSearchLocalNtpBackgroundFilename));
  base::WriteFile(path, "background_image", 16);
  base::ThreadPoolInstance::Get()->FlushForTesting();

  instant_service_->SelectLocalBackgroundImage(path);
  thread_bundle()->RunUntilIdle();

  EXPECT_TRUE(
      pref_service->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice));

  // Update theme info via Sync.
  pref_service->SetUserPref(
      prefs::kNtpCustomBackgroundDict,
      std::make_unique<base::Value>(GetBackgroundInfoAsDict(kUrl)));
  thread_bundle()->RunUntilIdle();

  ThemeBackgroundInfo* theme_info = instant_service_->GetInitializedThemeInfo();
  EXPECT_EQ(kUrl, theme_info->custom_background_url);
  EXPECT_FALSE(
      pref_service->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice));
  EXPECT_TRUE(instant_service_->IsCustomBackgroundSet());
}

TEST_F(InstantServiceTest, ValidateBackdropUrls) {
  ASSERT_FALSE(instant_service_->IsCustomBackgroundSet());
  const GURL kBackdropUrl1("https://www.foo.com");
  const GURL kBackdropUrl2("https://www.bar.com");
  const GURL kNonBackdropUrl1("https://www.test.com");
  const GURL kNonBackdropUrl2("https://www.foo.com/path");

  instant_service_->AddValidBackdropUrlForTesting(kBackdropUrl1);
  instant_service_->AddValidBackdropUrlForTesting(kBackdropUrl2);

  instant_service_->SetCustomBackgroundURL(kBackdropUrl1);
  ThemeBackgroundInfo* theme_info = instant_service_->GetInitializedThemeInfo();
  EXPECT_EQ(kBackdropUrl1, theme_info->custom_background_url);
  EXPECT_TRUE(instant_service_->IsCustomBackgroundSet());

  instant_service_->SetCustomBackgroundURL(kNonBackdropUrl1);
  theme_info = instant_service_->GetInitializedThemeInfo();
  EXPECT_EQ(GURL(), theme_info->custom_background_url);
  EXPECT_FALSE(instant_service_->IsCustomBackgroundSet());

  instant_service_->SetCustomBackgroundURL(kBackdropUrl2);
  theme_info = instant_service_->GetInitializedThemeInfo();
  EXPECT_EQ(kBackdropUrl2, theme_info->custom_background_url);
  EXPECT_TRUE(instant_service_->IsCustomBackgroundSet());

  instant_service_->SetCustomBackgroundURL(kNonBackdropUrl2);
  theme_info = instant_service_->GetInitializedThemeInfo();
  EXPECT_EQ(GURL(), theme_info->custom_background_url);
  EXPECT_FALSE(instant_service_->IsCustomBackgroundSet());
}

TEST_F(InstantServiceTest, TestNoThemeInfo) {
  instant_service_->theme_info_ = nullptr;
  EXPECT_NE(nullptr, instant_service_->GetInitializedThemeInfo());

  instant_service_->theme_info_ = nullptr;
  // As |FallbackToDefaultThemeInfo| uses |theme_info_| it should initialize it
  // otherwise the test should crash.
  instant_service_->FallbackToDefaultThemeInfo();
  EXPECT_NE(nullptr, instant_service_->theme_info_);
}

TEST_F(InstantServiceTest, TestResetToDefault) {
  MockInstantService mock_instant_service_(profile());
  EXPECT_CALL(mock_instant_service_, ResetCustomLinks());
  EXPECT_CALL(mock_instant_service_, ResetCustomBackgroundThemeInfo());
  mock_instant_service_.ResetToDefault();
}

class InstantServiceThemeTest : public InstantServiceTest {
 public:
  InstantServiceThemeTest() {}
  ~InstantServiceThemeTest() override {}

  ui::TestNativeTheme* theme() { return &theme_; }

 private:
  ui::TestNativeTheme theme_;

  DISALLOW_COPY_AND_ASSIGN(InstantServiceThemeTest);
};

TEST_F(InstantServiceThemeTest, DarkModeHandler) {
  testing::StrictMock<MockInstantServiceObserver> mock_observer;
  instant_service_->AddObserver(&mock_observer);
  theme()->SetDarkMode(false);
  instant_service_->SetDarkModeThemeForTesting(theme());

  // Enable dark mode.
  ThemeBackgroundInfo theme_info;
  EXPECT_CALL(mock_observer, ThemeInfoChanged(testing::_))
      .WillOnce(testing::SaveArg<0>(&theme_info));
  theme()->SetDarkMode(true);
  theme()->NotifyObservers();
  thread_bundle()->RunUntilIdle();

  EXPECT_TRUE(theme_info.using_dark_mode);

  // Disable dark mode.
  EXPECT_CALL(mock_observer, ThemeInfoChanged(testing::_))
      .WillOnce(testing::SaveArg<0>(&theme_info));
  theme()->SetDarkMode(false);
  theme()->NotifyObservers();
  thread_bundle()->RunUntilIdle();

  EXPECT_FALSE(theme_info.using_dark_mode);
}

TEST_F(InstantServiceTest, LocalImageDoesNotHaveAttribution) {
  ASSERT_FALSE(instant_service_->IsCustomBackgroundSet());
  const GURL kUrl("https://www.foo.com");
  const std::string kAttributionLine1 = "foo";
  const std::string kAttributionLine2 = "bar";
  const GURL kActionUrl("https://www.bar.com");

  sync_preferences::TestingPrefServiceSyncable* pref_service =
      profile()->GetTestingPrefService();
  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  instant_service_->AddValidBackdropUrlForTesting(kUrl);
  instant_service_->SetCustomBackgroundURLWithAttributions(
      kUrl, kAttributionLine1, kAttributionLine2, kActionUrl);

  ThemeBackgroundInfo* theme_info = instant_service_->GetInitializedThemeInfo();
  ASSERT_EQ(kAttributionLine1,
            theme_info->custom_background_attribution_line_1);
  ASSERT_EQ(kAttributionLine2,
            theme_info->custom_background_attribution_line_2);
  ASSERT_EQ(kActionUrl, theme_info->custom_background_attribution_action_url);
  ASSERT_TRUE(instant_service_->IsCustomBackgroundSet());

  base::FilePath profile_path = profile()->GetPath();
  base::FilePath path(profile_path.AppendASCII(
      chrome::kChromeSearchLocalNtpBackgroundFilename));
  base::WriteFile(path, "background_image", 16);
  base::ThreadPoolInstance::Get()->FlushForTesting();

  instant_service_->SelectLocalBackgroundImage(path);
  thread_bundle()->RunUntilIdle();

  theme_info = instant_service_->GetInitializedThemeInfo();
  EXPECT_TRUE(base::StartsWith(theme_info->custom_background_url.spec(),
                               chrome::kChromeSearchLocalNtpBackgroundUrl,
                               base::CompareCase::SENSITIVE));
  EXPECT_TRUE(
      pref_service->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice));
  EXPECT_TRUE(instant_service_->IsCustomBackgroundSet());
  EXPECT_EQ(std::string(), theme_info->custom_background_attribution_line_1);
  EXPECT_EQ(std::string(), theme_info->custom_background_attribution_line_2);
  EXPECT_EQ(GURL(), theme_info->custom_background_attribution_action_url);
}

TEST_F(InstantServiceTest, TestUpdateCustomBackgroundColor) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(32, 32);
  bitmap.eraseColor(SK_ColorRED);
  gfx::Image image = gfx::Image::CreateFrom1xBitmap(bitmap);
  sync_preferences::TestingPrefServiceSyncable* pref_service =
      profile()->GetTestingPrefService();

  ASSERT_FALSE(instant_service_->IsCustomBackgroundSet());

  // Background color will not update if no background is set.
  instant_service_->UpdateCustomBackgroundColorAsync(
      GURL(), image, image_fetcher::RequestMetadata());
  thread_bundle()->RunUntilIdle();
  EXPECT_FALSE(CheckBackgroundColor(
      SK_ColorRED,
      pref_service->GetDictionary(prefs::kNtpCustomBackgroundDict)));

  const GURL kUrl("https://www.foo.com");
  const std::string kAttributionLine1 = "foo";
  const std::string kAttributionLine2 = "bar";
  const GURL kActionUrl("https://www.bar.com");

  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  instant_service_->AddValidBackdropUrlForTesting(kUrl);
  instant_service_->SetCustomBackgroundURLWithAttributions(
      kUrl, kAttributionLine1, kAttributionLine2, kActionUrl);

  // Background color will not update if current background url changed.
  instant_service_->UpdateCustomBackgroundColorAsync(
      GURL("different_url"), image, image_fetcher::RequestMetadata());
  thread_bundle()->RunUntilIdle();
  EXPECT_FALSE(CheckBackgroundColor(
      SK_ColorRED,
      pref_service->GetDictionary(prefs::kNtpCustomBackgroundDict)));

  // Background color should update.
  instant_service_->UpdateCustomBackgroundColorAsync(
      kUrl, image, image_fetcher::RequestMetadata());
  thread_bundle()->RunUntilIdle();
  EXPECT_TRUE(CheckBackgroundColor(
      SK_ColorRED,
      pref_service->GetDictionary(prefs::kNtpCustomBackgroundDict)));
}
