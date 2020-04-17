// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_TAB_SPECIFIC_CONTENT_SETTINGS_DELEGATE_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_TAB_SPECIFIC_CONTENT_SETTINGS_DELEGATE_H_

#include "chrome/common/custom_handlers/protocol_handler.h"
#include "components/content_settings/browser/tab_specific_content_settings.h"

namespace chrome {

class TabSpecificContentSettingsDelegate
    : public content_settings::TabSpecificContentSettings::Delegate,
      public content::WebContentsObserver {
 public:
  explicit TabSpecificContentSettingsDelegate(
      content::WebContents* web_contents);
  ~TabSpecificContentSettingsDelegate() override;
  TabSpecificContentSettingsDelegate(
      const TabSpecificContentSettingsDelegate&) = delete;
  TabSpecificContentSettingsDelegate& operator=(
      const TabSpecificContentSettingsDelegate&) = delete;

  static TabSpecificContentSettingsDelegate* FromWebContents(
      content::WebContents* web_contents);

  // Call to indicate that there is a protocol handler pending user approval.
  void set_pending_protocol_handler(const ProtocolHandler& handler) {
    pending_protocol_handler_ = handler;
  }

  const ProtocolHandler& pending_protocol_handler() const {
    return pending_protocol_handler_;
  }

  void ClearPendingProtocolHandler() {
    pending_protocol_handler_ = ProtocolHandler::EmptyProtocolHandler();
  }

  // Sets the previous protocol handler which will be replaced by the
  // pending protocol handler.
  void set_previous_protocol_handler(const ProtocolHandler& handler) {
    previous_protocol_handler_ = handler;
  }

  const ProtocolHandler& previous_protocol_handler() const {
    return previous_protocol_handler_;
  }

  // Set whether the setting for the pending handler is DEFAULT (ignore),
  // ALLOW, or DENY.
  void set_pending_protocol_handler_setting(ContentSetting setting) {
    pending_protocol_handler_setting_ = setting;
  }

  ContentSetting pending_protocol_handler_setting() const {
    return pending_protocol_handler_setting_;
  }

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

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // The pending protocol handler, if any. This can be set if
  // registerProtocolHandler was invoked without user gesture.
  // The |IsEmpty| method will be true if no protocol handler is
  // pending registration.
  ProtocolHandler pending_protocol_handler_ =
      ProtocolHandler::EmptyProtocolHandler();

  // The previous protocol handler to be replaced by
  // the pending_protocol_handler_, if there is one. Empty if
  // there is no handler which would be replaced.
  ProtocolHandler previous_protocol_handler_ =
      ProtocolHandler::EmptyProtocolHandler();

  // The setting on the pending protocol handler registration. Persisted in case
  // the user opens the bubble and makes changes multiple times.
  ContentSetting pending_protocol_handler_setting_ = CONTENT_SETTING_DEFAULT;
};

}  // namespace chrome

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_TAB_SPECIFIC_CONTENT_SETTINGS_DELEGATE_H_
