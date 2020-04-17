// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_TAB_SPECIFIC_CONTENT_SETTINGS_DELEGATE_H_
#define WEBLAYER_BROWSER_TAB_SPECIFIC_CONTENT_SETTINGS_DELEGATE_H_

#include "components/content_settings/browser/tab_specific_content_settings.h"

namespace weblayer {

// Called by TabSpecificContentSettings to handle WebLayer specific logic.
class TabSpecificContentSettingsDelegate
    : public content_settings::TabSpecificContentSettings::Delegate {
 public:
  explicit TabSpecificContentSettingsDelegate(
      content::WebContents* web_contents);
  ~TabSpecificContentSettingsDelegate() override;
  TabSpecificContentSettingsDelegate(
      const TabSpecificContentSettingsDelegate&) = delete;
  TabSpecificContentSettingsDelegate& operator=(
      const TabSpecificContentSettingsDelegate&) = delete;

  static void UpdateRendererContentSettingRules(
      content::RenderProcessHost* process);

 private:
  // TabSpecificContentSettings::Delegate:
  void UpdateLocationBar() override;
  void SetContentSettingRules(
      content::RenderProcessHost* process,
      const RendererContentSettingRules& rules) override;
  PrefService* GetPrefs() override;
  HostContentSettingsMap* GetSettingsMap() override;
  std::vector<storage::FileSystemType> GetAdditionalFileSystemTypes() override;
  bool IsMicrophoneCameraStateChanged(
      content_settings::TabSpecificContentSettings::MicrophoneCameraState
          microphone_camera_state,
      const std::string& media_stream_selected_audio_device,
      const std::string& media_stream_selected_video_device) override;
  content_settings::TabSpecificContentSettings::MicrophoneCameraState
  GetMicrophoneCameraState() override;
  void OnContentBlocked(ContentSettingsType type) override;

  content::WebContents* web_contents_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_TAB_SPECIFIC_CONTENT_SETTINGS_DELEGATE_H_
