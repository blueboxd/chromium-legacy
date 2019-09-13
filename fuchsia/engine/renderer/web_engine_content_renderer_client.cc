// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/renderer/web_engine_content_renderer_client.h"

#include "base/feature_list.h"
#include "base/macros.h"
#include "components/cdm/renderer/widevine_key_system_properties.h"
#include "content/public/renderer/render_frame.h"
#include "fuchsia/engine/renderer/on_load_script_injector.h"
#include "fuchsia/engine/renderer/web_engine_url_loader_throttle_provider.h"
#include "fuchsia/engine/switches.h"
#include "media/base/eme_constants.h"
#include "media/base/video_codecs.h"
#include "services/network/public/cpp/features.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

WebEngineContentRendererClient::WebEngineContentRendererClient() = default;

WebEngineContentRendererClient::~WebEngineContentRendererClient() = default;

UrlRequestRulesReceiver*
WebEngineContentRendererClient::GetUrlRequestRulesReceiverForRenderFrameId(
    int render_frame_id) const {
  auto iter = url_request_receivers_by_id_.find(render_frame_id);
  DCHECK(iter != url_request_receivers_by_id_.end());
  return iter->second.get();
}

void WebEngineContentRendererClient::OnRenderFrameDeleted(int render_frame_id) {
  size_t count = url_request_receivers_by_id_.erase(render_frame_id);
  DCHECK_EQ(count, 1u);
}

void WebEngineContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  // Add WebEngine services to the new RenderFrame.
  // The objects' lifetimes are bound to the RenderFrame's lifetime.
  new OnLoadScriptInjector(render_frame);

  int render_frame_id = render_frame->GetRoutingID();
  auto rules_receiver = std::make_unique<UrlRequestRulesReceiver>(
      content::RenderFrame::FromRoutingID(render_frame_id),
      base::BindOnce(&WebEngineContentRendererClient::OnRenderFrameDeleted,
                     base::Unretained(this)));
  auto iter = url_request_receivers_by_id_.emplace(render_frame_id,
                                                   std::move(rules_receiver));
  DCHECK(iter.second);
}

std::unique_ptr<content::URLLoaderThrottleProvider>
WebEngineContentRendererClient::CreateURLLoaderThrottleProvider(
    content::URLLoaderThrottleProviderType type) {
  DCHECK(base::FeatureList::IsEnabled(network::features::kNetworkService));

  // TODO(crbug.com/976975): Add support for service workers.
  if (type == content::URLLoaderThrottleProviderType::kWorker)
    return nullptr;

  return std::make_unique<WebEngineURLLoaderThrottleProvider>(this);
}

void WebEngineContentRendererClient::AddSupportedKeySystems(
    std::vector<std::unique_ptr<media::KeySystemProperties>>* key_systems) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWidevine)) {
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
