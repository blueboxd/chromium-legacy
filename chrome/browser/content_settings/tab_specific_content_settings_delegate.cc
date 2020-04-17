// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/tab_specific_content_settings_delegate.h"

#include "chrome/browser/browsing_data/browsing_data_file_system_util.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/content_settings/browser/tab_specific_content_settings.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"

using content_settings::TabSpecificContentSettings;

namespace chrome {

TabSpecificContentSettingsDelegate::TabSpecificContentSettingsDelegate(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

TabSpecificContentSettingsDelegate::~TabSpecificContentSettingsDelegate() =
    default;

// static
TabSpecificContentSettingsDelegate*
TabSpecificContentSettingsDelegate::FromWebContents(
    content::WebContents* web_contents) {
  auto* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return nullptr;
  return static_cast<TabSpecificContentSettingsDelegate*>(
      content_settings->delegate());
}

void TabSpecificContentSettingsDelegate::UpdateLocationBar() {
  content_settings::UpdateLocationBarUiForWebContents(web_contents());
}

void TabSpecificContentSettingsDelegate::SetContentSettingRules(
    content::RenderProcessHost* process,
    const RendererContentSettingRules& rules) {
  // |channel| may be null in tests.
  IPC::ChannelProxy* channel = process->GetChannel();
  if (!channel)
    return;

  mojo::AssociatedRemote<chrome::mojom::RendererConfiguration> rc_interface;
  channel->GetRemoteAssociatedInterface(&rc_interface);
  rc_interface->SetContentSettingRules(rules);
}

PrefService* TabSpecificContentSettingsDelegate::GetPrefs() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (!profile)
    return nullptr;

  return profile->GetPrefs();
}

HostContentSettingsMap* TabSpecificContentSettingsDelegate::GetSettingsMap() {
  return HostContentSettingsMapFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
}

std::vector<storage::FileSystemType>
TabSpecificContentSettingsDelegate::GetAdditionalFileSystemTypes() {
  return browsing_data_file_system_util::GetAdditionalFileSystemTypes();
}

bool TabSpecificContentSettingsDelegate::IsMicrophoneCameraStateChanged(
    TabSpecificContentSettings::MicrophoneCameraState microphone_camera_state,
    const std::string& media_stream_selected_audio_device,
    const std::string& media_stream_selected_video_device) {
  PrefService* prefs = GetPrefs();
  scoped_refptr<MediaStreamCaptureIndicator> media_indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();

  if ((microphone_camera_state &
       TabSpecificContentSettings::MICROPHONE_ACCESSED) &&
      prefs->GetString(prefs::kDefaultAudioCaptureDevice) !=
          media_stream_selected_audio_device &&
      media_indicator->IsCapturingAudio(web_contents()))
    return true;

  if ((microphone_camera_state & TabSpecificContentSettings::CAMERA_ACCESSED) &&
      prefs->GetString(prefs::kDefaultVideoCaptureDevice) !=
          media_stream_selected_video_device &&
      media_indicator->IsCapturingVideo(web_contents()))
    return true;

  return false;
}

TabSpecificContentSettings::MicrophoneCameraState
TabSpecificContentSettingsDelegate::GetMicrophoneCameraState() {
  TabSpecificContentSettings::MicrophoneCameraState state =
      TabSpecificContentSettings::MICROPHONE_CAMERA_NOT_ACCESSED;

  // Include capture devices in the state if there are still consumers of the
  // approved media stream.
  scoped_refptr<MediaStreamCaptureIndicator> media_indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();
  if (media_indicator->IsCapturingAudio(web_contents()))
    state |= TabSpecificContentSettings::MICROPHONE_ACCESSED;
  if (media_indicator->IsCapturingVideo(web_contents()))
    state |= TabSpecificContentSettings::CAMERA_ACCESSED;

  return state;
}

void TabSpecificContentSettingsDelegate::OnContentBlocked(
    ContentSettingsType type) {
  if (type == ContentSettingsType::PLUGINS) {
    content_settings::RecordPluginsAction(
        content_settings::PLUGINS_ACTION_DISPLAYED_BLOCKED_ICON_IN_OMNIBOX);
  } else if (type == ContentSettingsType::POPUPS) {
    content_settings::RecordPopupsAction(
        content_settings::POPUPS_ACTION_DISPLAYED_BLOCKED_ICON_IN_OMNIBOX);
  }
}

void TabSpecificContentSettingsDelegate::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  ClearPendingProtocolHandler();

  if (web_contents()->GetVisibleURL().SchemeIsHTTPOrHTTPS()) {
    content_settings::RecordPluginsAction(
        content_settings::PLUGINS_ACTION_TOTAL_NAVIGATIONS);
  }
}

}  // namespace chrome
