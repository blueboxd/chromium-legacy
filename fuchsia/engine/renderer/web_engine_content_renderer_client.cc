// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/renderer/web_engine_content_renderer_client.h"

#include "base/macros.h"
#include "components/cdm/renderer/widevine_key_system_properties.h"
#include "content/public/renderer/render_frame.h"
#include "fuchsia/engine/renderer/on_load_script_injector.h"
#include "fuchsia/engine/switches.h"
#include "media/base/eme_constants.h"
#include "media/base/video_codecs.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

WebEngineContentRendererClient::WebEngineContentRendererClient() = default;

WebEngineContentRendererClient::~WebEngineContentRendererClient() = default;

void WebEngineContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  // Add WebEngine services to the new RenderFrame.
  // The objects' lifetimes are bound to the RenderFrame's lifetime.
  new OnLoadScriptInjector(render_frame);
}

void WebEngineContentRendererClient::AddSupportedKeySystems(
    std::vector<std::unique_ptr<media::KeySystemProperties>>* key_systems) {
  // TODO(yucliu): Check if Widevine is enabled from command line/context
  // feature flags.
  // TODO(yucliu): Check supported hw video decoders.
  media::SupportedCodecs supported_video_codecs = media::EME_CODEC_NONE;

  base::flat_set<media::EncryptionMode> encryption_schemes{
      media::EncryptionMode::kCenc, media::EncryptionMode::kCbcs};

  // Fuchsia always decrypts audio into clear buffers and return them back to
  // Chromium. Hardware secured decoders are only available for supported
  // video codecs.
  key_systems->emplace_back(new cdm::WidevineKeySystemProperties(
      media::EME_CODEC_AUDIO_ALL | supported_video_codecs,  // codecs
      encryption_schemes,      // encryption schemes
      supported_video_codecs,  // hw secure codecs
      encryption_schemes,      // hw secure encryption schemes
      cdm::WidevineKeySystemProperties::Robustness::
          HW_SECURE_CRYPTO,  // max audio robustness
      cdm::WidevineKeySystemProperties::Robustness::
          HW_SECURE_ALL,                            // max video robustness
      media::EmeSessionTypeSupport::NOT_SUPPORTED,  // persistent license
      media::EmeSessionTypeSupport::NOT_SUPPORTED,  // persistent usage record
      media::EmeFeatureSupport::ALWAYS_ENABLED,     // persistent state
      media::EmeFeatureSupport::ALWAYS_ENABLED));   // distinctive identifier
}

bool WebEngineContentRendererClient::IsSupportedVideoType(
    const media::VideoType& type) {
  // Fall back to default codec querying logic if software codecs aren't
  // disabled.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableSoftwareVideoDecoders)) {
    return ContentRendererClient::IsSupportedVideoType(type);
  }

  // TODO(fxb/36000): Replace these hardcoded checks with a query to the
  // fuchsia.mediacodec FIDL service.
  if (type.codec == media::kCodecH264 && type.level <= 41)
    return true;

  if (type.codec == media::kCodecVP9 && type.level <= 40)
    return true;

  return false;
}
