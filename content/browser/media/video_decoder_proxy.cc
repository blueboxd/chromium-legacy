// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/video_decoder_proxy.h"

#include "base/bind.h"
#include "base/logging.h"
#include "content/public/browser/system_connector.h"
#include "media/mojo/interfaces/constants.mojom.h"
#include "media/mojo/interfaces/media_service.mojom.h"
#include "media/mojo/interfaces/renderer_extensions.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

VideoDecoderProxy::VideoDecoderProxy() {
  DVLOG(1) << __func__;
}

VideoDecoderProxy::~VideoDecoderProxy() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void VideoDecoderProxy::Add(media::mojom::InterfaceFactoryRequest request) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  bindings_.AddBinding(this, std::move(request));
}

void VideoDecoderProxy::CreateAudioDecoder(
    media::mojom::AudioDecoderRequest request) {}

void VideoDecoderProxy::CreateVideoDecoder(
    media::mojom::VideoDecoderRequest request) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  InterfaceFactory* factory = GetMediaInterfaceFactory();
  if (factory)
    factory->CreateVideoDecoder(std::move(request));
}

void VideoDecoderProxy::CreateDefaultRenderer(
    const std::string& audio_device_id,
    media::mojom::RendererRequest request) {}

#if BUILDFLAG(ENABLE_CAST_RENDERER)
void VideoDecoderProxy::CreateCastRenderer(
    const base::UnguessableToken& overlay_plane_id,
    media::mojom::RendererRequest request) {}
#endif  // BUILDFLAG(ENABLE_CAST_RENDERER)

#if defined(OS_ANDROID)
void VideoDecoderProxy::CreateFlingingRenderer(
    const std::string& audio_device_id,
    media::mojom::FlingingRendererClientExtensionPtr client_extenion,
    media::mojom::RendererRequest request) {}

void VideoDecoderProxy::CreateMediaPlayerRenderer(
    media::mojom::MediaPlayerRendererClientExtensionPtr client_extension_ptr,
    media::mojom::RendererRequest request,
    media::mojom::MediaPlayerRendererExtensionRequest
        renderer_extension_request) {}
#endif  // defined(OS_ANDROID)

void VideoDecoderProxy::CreateCdm(
    const std::string& key_system,
    media::mojom::ContentDecryptionModuleRequest request) {}

void VideoDecoderProxy::CreateDecryptor(
    int cdm_id,
    media::mojom::DecryptorRequest request) {}

void VideoDecoderProxy::CreateCdmProxy(const base::Token& cdm_guid,
                                       media::mojom::CdmProxyRequest request) {}

media::mojom::InterfaceFactory* VideoDecoderProxy::GetMediaInterfaceFactory() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!interface_factory_ptr_)
    ConnectToMediaService();

  return interface_factory_ptr_.get();
}

void VideoDecoderProxy::ConnectToMediaService() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!interface_factory_ptr_);

  media::mojom::MediaServicePtr media_service;
  // TODO(slan): Use the BrowserContext Connector instead.
  // See https://crbug.com/638950.
  GetSystemConnector()->BindInterface(media::mojom::kMediaServiceName,
                                      &media_service);

  // TODO(sandersd): Do we need to bind an empty |interfaces| implementation?
  service_manager::mojom::InterfaceProviderPtr interfaces;
  media_service->CreateInterfaceFactory(MakeRequest(&interface_factory_ptr_),
                                        std::move(interfaces));

  interface_factory_ptr_.set_connection_error_handler(
      base::BindOnce(&VideoDecoderProxy::OnMediaServiceConnectionError,
                     base::Unretained(this)));
}

void VideoDecoderProxy::OnMediaServiceConnectionError() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  interface_factory_ptr_.reset();
}

}  // namespace content
