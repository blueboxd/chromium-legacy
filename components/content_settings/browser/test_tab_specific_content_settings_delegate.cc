// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/browser/test_tab_specific_content_settings_delegate.h"

namespace content_settings {

TestTabSpecificContentSettingsDelegate::TestTabSpecificContentSettingsDelegate(
    PrefService* prefs,
    HostContentSettingsMap* settings_map)
    : prefs_(prefs), settings_map_(settings_map) {}

TestTabSpecificContentSettingsDelegate::
    ~TestTabSpecificContentSettingsDelegate() = default;

void TestTabSpecificContentSettingsDelegate::UpdateLocationBar() {}

void TestTabSpecificContentSettingsDelegate::SetContentSettingRules(
    content::RenderProcessHost* process,
    const RendererContentSettingRules& rules) {}

PrefService* TestTabSpecificContentSettingsDelegate::GetPrefs() {
  return prefs_;
}

HostContentSettingsMap*
TestTabSpecificContentSettingsDelegate::GetSettingsMap() {
  return settings_map_.get();
}

ContentSetting TestTabSpecificContentSettingsDelegate::GetEmbargoSetting(
    const GURL& request_origin,
    ContentSettingsType permission) {
  return ContentSetting::CONTENT_SETTING_ASK;
}

std::vector<storage::FileSystemType>
TestTabSpecificContentSettingsDelegate::GetAdditionalFileSystemTypes() {
  return {};
}

browsing_data::CookieHelper::IsDeletionDisabledCallback
TestTabSpecificContentSettingsDelegate::GetIsDeletionDisabledCallback() {
  return base::NullCallback();
}

bool TestTabSpecificContentSettingsDelegate::IsMicrophoneCameraStateChanged(
    TabSpecificContentSettings::MicrophoneCameraState microphone_camera_state,
    const std::string& media_stream_selected_audio_device,
    const std::string& media_stream_selected_video_device) {
  return false;
}

TabSpecificContentSettings::MicrophoneCameraState
TestTabSpecificContentSettingsDelegate::GetMicrophoneCameraState() {
  return TabSpecificContentSettings::MICROPHONE_CAMERA_NOT_ACCESSED;
}

void TestTabSpecificContentSettingsDelegate::OnContentBlocked(
    ContentSettingsType type) {}

}  // namespace content_settings
