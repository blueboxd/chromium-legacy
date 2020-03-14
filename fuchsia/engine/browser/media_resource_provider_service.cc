// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/media_resource_provider_service.h"

#include <lib/fidl/cpp/interface_handle.h>
#include <lib/sys/cpp/component_context.h>

#include "base/bind.h"
#include "base/fuchsia/default_context.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/frame_service_base.h"
#include "content/public/browser/provision_fetcher_factory.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "fuchsia/engine/browser/frame_impl.h"
#include "fuchsia/engine/switches.h"
#include "media/base/provision_fetcher.h"
#include "media/fuchsia/cdm/service/fuchsia_cdm_manager.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

namespace {

class MediaResourceProviderImpl
    : public content::FrameServiceBase<
          media::mojom::FuchsiaMediaResourceProvider> {
 public:
  MediaResourceProviderImpl(
      media::FuchsiaCdmManager* cdm_manager,
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<media::mojom::FuchsiaMediaResourceProvider>
          receiver);
  ~MediaResourceProviderImpl() final;

  MediaResourceProviderImpl(const MediaResourceProviderImpl&) = delete;
  MediaResourceProviderImpl& operator=(const MediaResourceProviderImpl&) =
      delete;

  // media::mojom::FuchsiaMediaResourceProvider implementation.
  void CreateCdm(
      const std::string& key_system,
      fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
          request) final;
  void CreateAudioConsumer(
      fidl::InterfaceRequest<fuchsia::media::AudioConsumer> request) final;
  void CreateAudioCapturer(
      fidl::InterfaceRequest<fuchsia::media::AudioCapturer> request) final;

 private:
  media::FuchsiaCdmManager* const cdm_manager_;
};

MediaResourceProviderImpl::MediaResourceProviderImpl(
    media::FuchsiaCdmManager* cdm_manager,
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<media::mojom::FuchsiaMediaResourceProvider> receiver)
    : FrameServiceBase(render_frame_host, std::move(receiver)),
      cdm_manager_(cdm_manager) {
  DCHECK(cdm_manager_);
}

MediaResourceProviderImpl::~MediaResourceProviderImpl() = default;

void MediaResourceProviderImpl::CreateCdm(
    const std::string& key_system,
    fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
        request) {
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(
          render_frame_host()->GetProcess()->GetBrowserContext())
          ->GetURLLoaderFactoryForBrowserProcess();
  media::CreateFetcherCB create_fetcher_cb = base::BindRepeating(
      &content::CreateProvisionFetcher, std::move(loader_factory));
  cdm_manager_->CreateAndProvision(
      key_system, origin(), std::move(create_fetcher_cb), std::move(request));
}

void MediaResourceProviderImpl::CreateAudioConsumer(
    fidl::InterfaceRequest<fuchsia::media::AudioConsumer> request) {
  auto factory = base::fuchsia::ComponentContextForCurrentProcess()
                     ->svc()
                     ->Connect<fuchsia::media::SessionAudioConsumerFactory>();
  factory->CreateAudioConsumer(
      FrameImpl::FromRenderFrameHost(render_frame_host())->media_session_id(),
      std::move(request));
}

void MediaResourceProviderImpl::CreateAudioCapturer(
    fidl::InterfaceRequest<fuchsia::media::AudioCapturer> request) {
  if (FrameImpl::FromRenderFrameHost(render_frame_host())
          ->permission_controller()
          ->GetPermissionState(content::PermissionType::AUDIO_CAPTURE,
                               origin()) !=
      blink::mojom::PermissionStatus::GRANTED) {
    DLOG(WARNING)
        << "Received CreateAudioCapturer request from an origin that doesn't "
           "have AUDIO_CAPTURE permission.";
    return;
  }

  auto factory = base::fuchsia::ComponentContextForCurrentProcess()
                     ->svc()
                     ->Connect<fuchsia::media::Audio>();
  factory->CreateAudioCapturer(std::move(request), /*loopback=*/false);
}

class WidevineHandler : public media::FuchsiaCdmManager::KeySystemHandler {
 public:
  WidevineHandler() = default;
  ~WidevineHandler() override = default;

  void CreateCdm(
      fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
          request) override {
    auto widevine = base::fuchsia::ComponentContextForCurrentProcess()
                        ->svc()
                        ->Connect<fuchsia::media::drm::Widevine>();
    widevine->CreateContentDecryptionModule(std::move(request));
  }

  fuchsia::media::drm::ProvisionerPtr CreateProvisioner() override {
    fuchsia::media::drm::ProvisionerPtr provisioner;

    auto widevine = base::fuchsia::ComponentContextForCurrentProcess()
                        ->svc()
                        ->Connect<fuchsia::media::drm::Widevine>();
    widevine->CreateProvisioner(provisioner.NewRequest());

    return provisioner;
  }
};

class PlayreadyHandler : public media::FuchsiaCdmManager::KeySystemHandler {
 public:
  PlayreadyHandler() = default;
  ~PlayreadyHandler() override = default;

  void CreateCdm(
      fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
          request) override {
    auto playready = base::fuchsia::ComponentContextForCurrentProcess()
                         ->svc()
                         ->Connect<fuchsia::media::drm::PlayReady>();
    playready->CreateContentDecryptionModule(std::move(request));
  }

  fuchsia::media::drm::ProvisionerPtr CreateProvisioner() override {
    // Provisioning is not required for PlayReady.
    return fuchsia::media::drm::ProvisionerPtr();
  }
};

std::unique_ptr<media::FuchsiaCdmManager> CreateCdmManager() {
  media::FuchsiaCdmManager::KeySystemHandlerMap handlers;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWidevine)) {
    handlers.emplace(kWidevineKeySystem, std::make_unique<WidevineHandler>());
  }

  std::string playready_key_system =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kPlayreadyKeySystem);
  if (!playready_key_system.empty()) {
    handlers.emplace(playready_key_system,
                     std::make_unique<PlayreadyHandler>());
  }

  return std::make_unique<media::FuchsiaCdmManager>(std::move(handlers));
}

}  // namespace

MediaResourceProviderService::MediaResourceProviderService()
    : cdm_manager_(CreateCdmManager()) {}

MediaResourceProviderService::~MediaResourceProviderService() = default;

void MediaResourceProviderService::Bind(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<media::mojom::FuchsiaMediaResourceProvider>
        receiver) {
  // The object will delete itself when connection to the frame is broken.
  new MediaResourceProviderImpl(cdm_manager_.get(), frame_host,
                                std::move(receiver));
}
