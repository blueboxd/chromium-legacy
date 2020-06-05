// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_BROWSER_TEST_TAB_SPECIFIC_CONTENT_SETTINGS_DELEGATE_H_
#define COMPONENTS_CONTENT_SETTINGS_BROWSER_TEST_TAB_SPECIFIC_CONTENT_SETTINGS_DELEGATE_H_

#include "base/memory/scoped_refptr.h"
#include "components/content_settings/browser/tab_specific_content_settings.h"

namespace content_settings {

class TestTabSpecificContentSettingsDelegate
    : public TabSpecificContentSettings::Delegate {
 public:
  TestTabSpecificContentSettingsDelegate(PrefService* prefs,
                                         HostContentSettingsMap* settings_map);
  ~TestTabSpecificContentSettingsDelegate() override;

  // TabSpecificContentSettings::Delegate:
  void UpdateLocationBar() override;
  void SetContentSettingRules(
      content::RenderProcessHost* process,
      const RendererContentSettingRules& rules) override;
  PrefService* GetPrefs() override;
  HostContentSettingsMap* GetSettingsMap() override;
  ContentSetting GetEmbargoSetting(const GURL& request_origin,
                                   ContentSettingsType permission) override;
  std::vector<storage::FileSystemType> GetAdditionalFileSystemTypes() override;
  browsing_data::CookieHelper::IsDeletionDisabledCallback
  GetIsDeletionDisabledCallback() override;
  bool IsMicrophoneCameraStateChanged(
      TabSpecificContentSettings::MicrophoneCameraState microphone_camera_state,
      const std::string& media_stream_selected_audio_device,
      const std::string& media_stream_selected_video_device) override;
  TabSpecificContentSettings::MicrophoneCameraState GetMicrophoneCameraState()
      override;
  void OnContentBlocked(ContentSettingsType type) override;

 private:
  PrefService* prefs_;
  scoped_refptr<HostContentSettingsMap> settings_map_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_BROWSER_TEST_TAB_SPECIFIC_CONTENT_SETTINGS_DELEGATE_H_
