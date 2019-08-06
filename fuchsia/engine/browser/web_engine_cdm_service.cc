// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/web_engine_cdm_service.h"

#include <fuchsia/media/drm/cpp/fidl.h>
#include <string>

#include "base/bind.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/frame_service_base.h"
#include "content/public/browser/provision_fetcher_factory.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "media/base/provision_fetcher.h"
#include "media/fuchsia/mojom/fuchsia_cdm_provider.mojom.h"

namespace {
class FuchsiaCdmProviderImpl
    : public content::FrameServiceBase<media::mojom::FuchsiaCdmProvider> {
 public:
  FuchsiaCdmProviderImpl(media::FuchsiaCdmManager* cdm_manager,
                         media::CreateFetcherCB create_fetcher_cb,
                         content::RenderFrameHost* render_frame_host,
                         media::mojom::FuchsiaCdmProviderRequest request);
  ~FuchsiaCdmProviderImpl() final;

  // media::mojom::FuchsiaCdmProvider implementation.
  void CreateCdmInterface(
      const std::string& key_system,
      fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
          request) final;

 private:
  media::FuchsiaCdmManager* const cdm_manager_;
  const media::CreateFetcherCB create_fetcher_cb_;

  DISALLOW_COPY_AND_ASSIGN(FuchsiaCdmProviderImpl);
};

FuchsiaCdmProviderImpl::FuchsiaCdmProviderImpl(
    media::FuchsiaCdmManager* cdm_manager,
    media::CreateFetcherCB create_fetcher_cb,
    content::RenderFrameHost* render_frame_host,
    media::mojom::FuchsiaCdmProviderRequest request)
    : FrameServiceBase(render_frame_host, std::move(request)),
      cdm_manager_(cdm_manager),
      create_fetcher_cb_(std::move(create_fetcher_cb)) {
  DCHECK(cdm_manager_);
}

FuchsiaCdmProviderImpl::~FuchsiaCdmProviderImpl() = default;

void FuchsiaCdmProviderImpl::CreateCdmInterface(
    const std::string& key_system,
    fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
        request) {
  cdm_manager_->CreateAndProvision(key_system, origin(), create_fetcher_cb_,
                                   std::move(request));
}

void BindFuchsiaCdmProvider(media::FuchsiaCdmManager* cdm_manager,
                            media::mojom::FuchsiaCdmProviderRequest request,
                            content::RenderFrameHost* const frame_host) {
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(
          frame_host->GetProcess()->GetBrowserContext())
          ->GetURLLoaderFactoryForBrowserProcess();

  // The object will delete itself when connection to the frame is broken.
  new FuchsiaCdmProviderImpl(
      cdm_manager,
      base::BindRepeating(&content::CreateProvisionFetcher,
                          std::move(loader_factory)),
      frame_host, std::move(request));
}
}  // namespace

WebEngineCdmService::WebEngineCdmService(
    service_manager::BinderRegistryWithArgs<content::RenderFrameHost*>*
        registry)
    : registry_(registry) {
  DCHECK(registry_);
  registry_->AddInterface(
      base::BindRepeating(&BindFuchsiaCdmProvider, &cdm_manager_));
}

WebEngineCdmService::~WebEngineCdmService() {
  registry_->RemoveInterface<media::mojom::FuchsiaCdmProvider>();
}
