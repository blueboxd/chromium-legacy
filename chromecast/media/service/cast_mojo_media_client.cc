// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/service/cast_mojo_media_client.h"

#include "chromecast/media/cma/backend/cma_backend_factory.h"
#include "chromecast/media/service/cast_renderer.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "media/base/cdm_factory.h"
#include "media/base/media_log.h"
#include "media/base/overlay_info.h"

namespace chromecast {
namespace media {

CastMojoMediaClient::CastMojoMediaClient(
    CmaBackendFactory* backend_factory,
    const CreateCdmFactoryCB& create_cdm_factory_cb,
    VideoModeSwitcher* video_mode_switcher,
    VideoResolutionPolicy* video_resolution_policy)
    : connector_(nullptr),
      backend_factory_(backend_factory),
      create_cdm_factory_cb_(create_cdm_factory_cb),
      video_mode_switcher_(video_mode_switcher),
      video_resolution_policy_(video_resolution_policy) {
  DCHECK(backend_factory_);
}

CastMojoMediaClient::~CastMojoMediaClient() {}

void CastMojoMediaClient::Initialize(service_manager::Connector* connector) {
  DCHECK(!connector_);
  DCHECK(connector);
  connector_ = connector;
}

#if BUILDFLAG(ENABLE_CAST_RENDERER)
std::unique_ptr<::media::Renderer> CastMojoMediaClient::CreateCastRenderer(
    service_manager::mojom::InterfaceProvider* host_interfaces,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    ::media::MediaLog* /* media_log */,
    const base::UnguessableToken& /* overlay_plane_id */) {
  // TODO(crbug.com/925450): 1) Make a VideoGeometrySetter within
  // CastMojoMediaClient. 2) Before return the created CastRenderer, pass the
  // CastRenderer and the |overlay_plane_id| to VideoGeometrySetter so it can
  // maintain a map between the CastRenderers and their associated
  // |overlay_plane_id|s.
  return std::make_unique<CastRenderer>(
      backend_factory_, task_runner, video_mode_switcher_,
      video_resolution_policy_, connector_, host_interfaces);
}
#endif

std::unique_ptr<::media::Renderer> CastMojoMediaClient::CreateRenderer(
    service_manager::mojom::InterfaceProvider* host_interfaces,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    ::media::MediaLog* /* media_log */,
    const std::string& audio_device_id) {
  return std::make_unique<CastRenderer>(
      backend_factory_, task_runner, video_mode_switcher_,
      video_resolution_policy_, connector_, host_interfaces);
}

std::unique_ptr<::media::CdmFactory> CastMojoMediaClient::CreateCdmFactory(
    service_manager::mojom::InterfaceProvider* host_interfaces) {
  return create_cdm_factory_cb_.Run(host_interfaces);
}

}  // namespace media
}  // namespace chromecast
