// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/browser/content_settings_usages_state.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_result.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestDelegate
    : public content_settings::TabSpecificContentSettings::Delegate {
 public:
  explicit TestDelegate(
      HostContentSettingsMap* map,
      permissions::PermissionDecisionAutoBlocker* auto_blocker)
      : content_settings_map_(map), auto_blocker_(auto_blocker) {}

 private:
  // content_settings::TabSpecificContentSettings::Delegate:
  void UpdateLocationBar() override {}
  void SetContentSettingRules(
      content::RenderProcessHost* process,
      const RendererContentSettingRules& rules) override {}
  PrefService* GetPrefs() override { return nullptr; }
  HostContentSettingsMap* GetSettingsMap() override {
    return content_settings_map_;
  }
  ContentSetting GetEmbargoSetting(const GURL& request_origin,
                                   ContentSettingsType permission) override {
    return auto_blocker_->GetEmbargoResult(request_origin, permission)
        .content_setting;
  }
  std::vector<storage::FileSystemType> GetAdditionalFileSystemTypes() override {
    return {};
  }
  browsing_data::CookieHelper::IsDeletionDisabledCallback
  GetIsDeletionDisabledCallback() override {
    return base::NullCallback();
  }
  bool IsMicrophoneCameraStateChanged(
      content_settings::TabSpecificContentSettings::MicrophoneCameraState
          microphone_camera_state,
      const std::string& media_stream_selected_audio_device,
      const std::string& media_stream_selected_video_device) override {
    return false;
  }
  content_settings::TabSpecificContentSettings::MicrophoneCameraState
  GetMicrophoneCameraState() override {
    return content_settings::TabSpecificContentSettings::
        MICROPHONE_CAMERA_NOT_ACCESSED;
  }
  void OnContentBlocked(ContentSettingsType type) override {}

  HostContentSettingsMap* content_settings_map_;
  permissions::PermissionDecisionAutoBlocker* auto_blocker_;
};

class ContentSettingsUsagesStateTests : public testing::Test {
 public:
  ContentSettingsUsagesStateTests() = default;

  void SetUp() override {
    auto_blocker_ =
        PermissionDecisionAutoBlockerFactory::GetForProfile(&profile_);
    content_settings_map_ =
        HostContentSettingsMapFactory::GetForProfile(&profile_);
    delegate_ =
        std::make_unique<TestDelegate>(content_settings_map_, auto_blocker_);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  HostContentSettingsMap* content_settings_map_;
  permissions::PermissionDecisionAutoBlocker* auto_blocker_;
  std::unique_ptr<TestDelegate> delegate_;

  void ClearOnNewOrigin(ContentSettingsType type) {
    ContentSettingsUsagesState state(delegate_.get(), type);
    const GURL url_0("http://www.example.com");

    state.DidNavigate(url_0, GURL());

    content_settings_map_->SetContentSettingDefaultScope(
        url_0, url_0, type, std::string(), CONTENT_SETTING_ALLOW);
    state.OnPermissionSet(url_0, true);

    const GURL url_1("http://www.example1.com");
    content_settings_map_->SetContentSettingDefaultScope(
        url_1, url_0, type, std::string(), CONTENT_SETTING_BLOCK);
    state.OnPermissionSet(url_1, false);

    const ContentSettingsUsagesState::StateMap state_map = state.state_map();
    EXPECT_EQ(2U, state_map.size());

    ContentSettingsUsagesState::FormattedHostsPerState formatted_host_per_state;
    unsigned int tab_state_flags = 0;
    state.GetDetailedInfo(&formatted_host_per_state, &tab_state_flags);
    EXPECT_TRUE(tab_state_flags &
                ContentSettingsUsagesState::TABSTATE_HAS_ANY_ALLOWED)
        << tab_state_flags;
    EXPECT_TRUE(tab_state_flags &
                ContentSettingsUsagesState::TABSTATE_HAS_EXCEPTION)
        << tab_state_flags;
    EXPECT_FALSE(tab_state_flags &
                 ContentSettingsUsagesState::TABSTATE_HAS_CHANGED)
        << tab_state_flags;
    EXPECT_TRUE(tab_state_flags &
                ContentSettingsUsagesState::TABSTATE_HAS_ANY_ICON)
        << tab_state_flags;
    EXPECT_EQ(1U, formatted_host_per_state[CONTENT_SETTING_ALLOW].size());
    EXPECT_EQ(1U, formatted_host_per_state[CONTENT_SETTING_ALLOW].count(
                      url_0.host()));

    EXPECT_EQ(1U, formatted_host_per_state[CONTENT_SETTING_BLOCK].size());
    EXPECT_EQ(1U, formatted_host_per_state[CONTENT_SETTING_BLOCK].count(
                      url_1.host()));

    state.OnPermissionSet(url_0, false);

    formatted_host_per_state.clear();
    tab_state_flags = 0;
    state.GetDetailedInfo(&formatted_host_per_state, &tab_state_flags);
    EXPECT_FALSE(tab_state_flags &
                 ContentSettingsUsagesState::TABSTATE_HAS_ANY_ALLOWED)
        << tab_state_flags;
    EXPECT_TRUE(tab_state_flags &
                ContentSettingsUsagesState::TABSTATE_HAS_EXCEPTION)
        << tab_state_flags;
    EXPECT_TRUE(tab_state_flags &
                ContentSettingsUsagesState::TABSTATE_HAS_CHANGED)
        << tab_state_flags;
    EXPECT_TRUE(tab_state_flags &
                ContentSettingsUsagesState::TABSTATE_HAS_ANY_ICON)
        << tab_state_flags;
    EXPECT_EQ(0U, formatted_host_per_state[CONTENT_SETTING_ALLOW].size());
    EXPECT_EQ(2U, formatted_host_per_state[CONTENT_SETTING_BLOCK].size());
    EXPECT_EQ(1U, formatted_host_per_state[CONTENT_SETTING_BLOCK].count(
                      url_0.host()));
    EXPECT_EQ(1U, formatted_host_per_state[CONTENT_SETTING_BLOCK].count(
                      url_1.host()));

    state.OnPermissionSet(url_0, true);

    state.DidNavigate(url_0, url_0);

    ContentSettingsUsagesState::StateMap new_state_map = state.state_map();
    EXPECT_EQ(state_map.size(), new_state_map.size());

    state.DidNavigate(GURL("http://foo.com/"), url_0);

    EXPECT_TRUE(state.state_map().empty());

    formatted_host_per_state.clear();
    tab_state_flags = 0;
    state.GetDetailedInfo(&formatted_host_per_state, &tab_state_flags);
    EXPECT_TRUE(formatted_host_per_state.empty());
    EXPECT_EQ(0U, tab_state_flags);
  }

  void ShowPortOnSameHost(ContentSettingsType type) {
    ContentSettingsUsagesState state(delegate_.get(), type);

    const GURL url_0("http://www.example.com");

    state.DidNavigate(url_0, GURL());

    content_settings_map_->SetContentSettingDefaultScope(
        url_0, url_0, type, std::string(), CONTENT_SETTING_ALLOW);
    state.OnPermissionSet(url_0, true);

    const GURL url_1("https://www.example.com");
    content_settings_map_->SetContentSettingDefaultScope(
        url_1, url_0, type, std::string(), CONTENT_SETTING_ALLOW);
    state.OnPermissionSet(url_1, true);

    const GURL url_2("http://www.example1.com");
    content_settings_map_->SetContentSettingDefaultScope(
        url_2, url_0, type, std::string(), CONTENT_SETTING_ALLOW);
    state.OnPermissionSet(url_2, true);

    const ContentSettingsUsagesState::StateMap state_map = state.state_map();
    EXPECT_EQ(3U, state_map.size());

    ContentSettingsUsagesState::FormattedHostsPerState formatted_host_per_state;
    unsigned int tab_state_flags = 0;
    state.GetDetailedInfo(&formatted_host_per_state, &tab_state_flags);

    EXPECT_EQ(3U, formatted_host_per_state[CONTENT_SETTING_ALLOW].size());
    EXPECT_EQ(1U, formatted_host_per_state[CONTENT_SETTING_ALLOW].count(
                      url_0.spec()));
    EXPECT_EQ(1U, formatted_host_per_state[CONTENT_SETTING_ALLOW].count(
                      url_1.spec()));
    EXPECT_EQ(1U, formatted_host_per_state[CONTENT_SETTING_ALLOW].count(
                      url_2.host()));

    state.OnPermissionSet(url_1, false);
    formatted_host_per_state.clear();
    tab_state_flags = 0;
    state.GetDetailedInfo(&formatted_host_per_state, &tab_state_flags);

    EXPECT_EQ(2U, formatted_host_per_state[CONTENT_SETTING_ALLOW].size());
    EXPECT_EQ(1U, formatted_host_per_state[CONTENT_SETTING_ALLOW].count(
                      url_0.spec()));
    EXPECT_EQ(1U, formatted_host_per_state[CONTENT_SETTING_ALLOW].count(
                      url_2.host()));
    EXPECT_EQ(1U, formatted_host_per_state[CONTENT_SETTING_BLOCK].size());
    EXPECT_EQ(1U, formatted_host_per_state[CONTENT_SETTING_BLOCK].count(
                      url_1.spec()));
  }

};  // namespace

TEST_F(ContentSettingsUsagesStateTests, ClearOnNewOriginForGeolocation) {
  ClearOnNewOrigin(ContentSettingsType::GEOLOCATION);
}

TEST_F(ContentSettingsUsagesStateTests, ClearOnNewOriginForMidi) {
  ClearOnNewOrigin(ContentSettingsType::MIDI_SYSEX);
}

TEST_F(ContentSettingsUsagesStateTests, ShowPortOnSameHostForGeolocation) {
  ShowPortOnSameHost(ContentSettingsType::GEOLOCATION);
}

TEST_F(ContentSettingsUsagesStateTests, ShowPortOnSameHostForMidi) {
  ShowPortOnSameHost(ContentSettingsType::MIDI_SYSEX);
}

TEST_F(ContentSettingsUsagesStateTests, GetDetailedInfo) {
  // Verify an origin with blocked |ContentSettingsType::GEOLOCATION| shown as
  // |ContentSettingsUsagesState::TABSTATE_HAS_EXCEPTION|.
  {
    ContentSettingsUsagesState state(delegate_.get(),
                                     ContentSettingsType::GEOLOCATION);

    const GURL origin_to_block("http://www.example.com");

    content_settings_map_->SetContentSettingDefaultScope(
        origin_to_block, origin_to_block, ContentSettingsType::GEOLOCATION,
        std::string(), CONTENT_SETTING_BLOCK);
    state.OnPermissionSet(origin_to_block, false);

    // Call |ContentSettingsUsagesState::DidNavigate| to initialize
    // |ContentSettingsUsagesState::embedder_url_|.
    state.DidNavigate(origin_to_block, origin_to_block);

    ContentSettingsUsagesState::FormattedHostsPerState formatted_host_per_state;
    unsigned int tab_state_flags = 0;
    state.GetDetailedInfo(&formatted_host_per_state, &tab_state_flags);

    EXPECT_TRUE(tab_state_flags &
                ContentSettingsUsagesState::TABSTATE_HAS_EXCEPTION);
  }

  // Verify an origin with embargoed |ContentSettingsType::GEOLOCATION| shown as
  // |ContentSettingsUsagesState::TABSTATE_HAS_EXCEPTION|.
  {
    ContentSettingsUsagesState state(delegate_.get(),
                                     ContentSettingsType::GEOLOCATION);

    const GURL origin_to_embargo("http://www.google.com");
    for (int i = 0; i < 3; ++i) {
      auto_blocker_->RecordDismissAndEmbargo(
          origin_to_embargo, ContentSettingsType::GEOLOCATION, false);
    }

    // Call |ContentSettingsUsagesState::DidNavigate| to initialize
    // |ContentSettingsUsagesState::embedder_url_|.
    state.DidNavigate(origin_to_embargo, origin_to_embargo);

    state.OnPermissionSet(origin_to_embargo, false);

    ContentSettingsUsagesState::FormattedHostsPerState formatted_host_per_state;
    unsigned int tab_state_flags = 0;
    state.GetDetailedInfo(&formatted_host_per_state, &tab_state_flags);

    EXPECT_TRUE(tab_state_flags &
                ContentSettingsUsagesState::TABSTATE_HAS_EXCEPTION);
  }
}

TEST_F(ContentSettingsUsagesStateTests, OriginEmbargoedWhileDefaultIsBlock) {
  content_settings_map_->SetDefaultContentSetting(
      ContentSettingsType::GEOLOCATION, CONTENT_SETTING_BLOCK);

  const GURL origin_to_embargo("http://www.example.com");

  for (int i = 0; i < 3; ++i) {
    auto_blocker_->RecordDismissAndEmbargo(
        origin_to_embargo, ContentSettingsType::GEOLOCATION, false);
  }

  ContentSettingsUsagesState state(delegate_.get(),
                                   ContentSettingsType::GEOLOCATION);

  state.OnPermissionSet(origin_to_embargo, false);
  // Call |ContentSettingsUsagesState::DidNavigate| to initialize
  // |ContentSettingsUsagesState::embedder_url_|.
  state.DidNavigate(origin_to_embargo, origin_to_embargo);
  ContentSettingsUsagesState::FormattedHostsPerState formatted_host_per_state;
  unsigned int tab_state_flags = 0;
  state.GetDetailedInfo(&formatted_host_per_state, &tab_state_flags);

  // No |TABSTATE_HAS_EXCEPTION| for embargoed origin because
  // CONTENT_SETTING_BLOCK is default.
  EXPECT_FALSE(tab_state_flags &
               ContentSettingsUsagesState::TABSTATE_HAS_EXCEPTION);
  EXPECT_FALSE(tab_state_flags &
               ContentSettingsUsagesState::TABSTATE_HAS_CHANGED);
  EXPECT_TRUE(tab_state_flags &
              ContentSettingsUsagesState::TABSTATE_HAS_ANY_ICON);
}

TEST_F(ContentSettingsUsagesStateTests, OriginEmbargoedWhileDefaultIsAsk) {
  const GURL origin_to_embargo("http://www.example.com");
  for (int i = 0; i < 3; ++i) {
    auto_blocker_->RecordDismissAndEmbargo(
        origin_to_embargo, ContentSettingsType::GEOLOCATION, false);
  }

  ContentSettingsUsagesState state(delegate_.get(),
                                   ContentSettingsType::GEOLOCATION);
  state.OnPermissionSet(origin_to_embargo, false);
  // Call |ContentSettingsUsagesState::DidNavigate| to initialize
  // |ContentSettingsUsagesState::embedder_url_|.
  state.DidNavigate(origin_to_embargo, origin_to_embargo);
  ContentSettingsUsagesState::FormattedHostsPerState formatted_host_per_state;
  unsigned int tab_state_flags = 0;
  state.GetDetailedInfo(&formatted_host_per_state, &tab_state_flags);

  // Has |TABSTATE_HAS_EXCEPTION| due to embargo.
  EXPECT_TRUE(tab_state_flags &
              ContentSettingsUsagesState::TABSTATE_HAS_EXCEPTION);
  // No |TABSTATE_HAS_CHANGED| flag because ContentSettings is
  // |CONTENT_SETTING_ASK| and origin is under embargo.
  EXPECT_FALSE(tab_state_flags &
               ContentSettingsUsagesState::TABSTATE_HAS_CHANGED);
  EXPECT_TRUE(tab_state_flags &
              ContentSettingsUsagesState::TABSTATE_HAS_ANY_ICON);
}
}  // namespace
