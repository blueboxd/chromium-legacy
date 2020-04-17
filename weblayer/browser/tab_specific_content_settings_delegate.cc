// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/tab_specific_content_settings_delegate.h"

#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/render_process_host.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/host_content_settings_map_factory.h"
#include "weblayer/common/renderer_configuration.mojom.h"

namespace weblayer {
namespace {

void SetContentSettingRules(content::RenderProcessHost* process,
                            const RendererContentSettingRules& rules) {
  mojo::AssociatedRemote<mojom::RendererConfiguration> rc_interface;
  process->GetChannel()->GetRemoteAssociatedInterface(&rc_interface);
  rc_interface->SetContentSettingRules(rules);
}

}  // namespace

TabSpecificContentSettingsDelegate::TabSpecificContentSettingsDelegate(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

TabSpecificContentSettingsDelegate::~TabSpecificContentSettingsDelegate() =
    default;

// static
void TabSpecificContentSettingsDelegate::UpdateRendererContentSettingRules(
    content::RenderProcessHost* process) {
  RendererContentSettingRules rules;
  GetRendererContentSettingRules(
      HostContentSettingsMapFactory::GetForBrowserContext(
          process->GetBrowserContext()),
      &rules);
  weblayer::SetContentSettingRules(process, rules);
}

void TabSpecificContentSettingsDelegate::UpdateLocationBar() {}

void TabSpecificContentSettingsDelegate::SetContentSettingRules(
    content::RenderProcessHost* process,
    const RendererContentSettingRules& rules) {
  weblayer::SetContentSettingRules(process, rules);
}

PrefService* TabSpecificContentSettingsDelegate::GetPrefs() {
  return static_cast<BrowserContextImpl*>(web_contents_->GetBrowserContext())
      ->pref_service();
}

HostContentSettingsMap* TabSpecificContentSettingsDelegate::GetSettingsMap() {
  return HostContentSettingsMapFactory::GetForBrowserContext(
      web_contents_->GetBrowserContext());
}

std::vector<storage::FileSystemType>
TabSpecificContentSettingsDelegate::GetAdditionalFileSystemTypes() {
  return {};
}

bool TabSpecificContentSettingsDelegate::IsMicrophoneCameraStateChanged(
    content_settings::TabSpecificContentSettings::MicrophoneCameraState
        microphone_camera_state,
    const std::string& media_stream_selected_audio_device,
    const std::string& media_stream_selected_video_device) {
  return false;
}

content_settings::TabSpecificContentSettings::MicrophoneCameraState
TabSpecificContentSettingsDelegate::GetMicrophoneCameraState() {
  return content_settings::TabSpecificContentSettings::
      MICROPHONE_CAMERA_NOT_ACCESSED;
}

void TabSpecificContentSettingsDelegate::OnContentBlocked(
    ContentSettingsType type) {}

}  // namespace weblayer
