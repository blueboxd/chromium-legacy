// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/mirroring_service.h"

#include "base/bind.h"
#include "base/callback.h"
#include "components/mirroring/service/session.h"
#include "services/viz/public/cpp/gpu/gpu.h"

namespace mirroring {

MirroringService::MirroringService(
    service_manager::mojom::ServiceRequest request,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : service_binding_(this, std::move(request)),
      service_keepalive_(&service_binding_, base::TimeDelta()),
      io_task_runner_(std::move(io_task_runner)) {
  registry_.AddInterface<mojom::MirroringService>(
      base::BindRepeating(&MirroringService::Create, base::Unretained(this)));
}

MirroringService::~MirroringService() {
  session_.reset();
  registry_.RemoveInterface<mojom::MirroringService>();
}

void MirroringService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

bool MirroringService::OnServiceManagerConnectionLost() {
  bindings_.CloseAllBindings();
  return true;
}

void MirroringService::Create(mojom::MirroringServiceRequest request) {
  bindings_.AddBinding(this, std::move(request));
  bindings_.set_connection_error_handler(base::BindRepeating(
      [](MirroringService* service) {
        service->session_.reset();
        service->bindings_.CloseAllBindings();
      },
      this));
}

void MirroringService::Start(mojom::SessionParametersPtr params,
                             const gfx::Size& max_resolution,
                             mojom::SessionObserverPtr observer,
                             mojom::ResourceProviderPtr resource_provider,
                             mojom::CastMessageChannelPtr outbound_channel,
                             mojom::CastMessageChannelRequest inbound_channel) {
  session_.reset();  // Stops the current session if active.
  std::unique_ptr<viz::Gpu> gpu;
  if (params->type != mojom::SessionType::AUDIO_ONLY) {
    gpu = viz::Gpu::Create(service_binding_.GetConnector(), "content_system",
                           io_task_runner_);
  }
  session_ = std::make_unique<Session>(
      std::move(params), max_resolution, std::move(observer),
      std::move(resource_provider), std::move(outbound_channel),
      std::move(inbound_channel), std::move(gpu));
}

}  // namespace mirroring
