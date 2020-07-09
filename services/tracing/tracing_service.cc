// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/tracing_service.h"

#include <utility>

#include "base/bind.h"
#include "services/tracing/perfetto/consumer_host.h"
#include "services/tracing/perfetto/perfetto_service.h"
#include "services/tracing/public/mojom/traced_process.mojom.h"

namespace tracing {

namespace {

void OnProcessConnected(
    PerfettoService* perfetto_service,
    mojo::Remote<mojom::TracedProcess> traced_process,
    uint32_t pid,
    mojo::PendingReceiver<mojom::PerfettoService> service_receiver) {
  perfetto_service->BindReceiver(std::move(service_receiver), pid);
}

}  // namespace

TracingService::TracingService(PerfettoService* perfetto_service)
    : perfetto_service_(perfetto_service ? perfetto_service
                                         : PerfettoService::GetInstance()) {}

TracingService::TracingService(
    mojo::PendingReceiver<mojom::TracingService> receiver)
    : receiver_(this, std::move(receiver)),
      perfetto_service_(PerfettoService::GetInstance()) {}

TracingService::~TracingService() = default;

void TracingService::Initialize(std::vector<mojom::ClientInfoPtr> clients) {
  for (auto& client : clients) {
    AddClient(std::move(client));
  }
  perfetto_service_->SetActiveServicePidsInitialized();
}

void TracingService::AddClient(mojom::ClientInfoPtr client) {
  perfetto_service_->AddActiveServicePid(client->pid);

  mojo::Remote<mojom::TracedProcess> process(std::move(client->process));
  auto new_connection_request = mojom::ConnectToTracingRequest::New();
  auto service_receiver =
      new_connection_request->perfetto_service.InitWithNewPipeAndPassReceiver();
  mojom::TracedProcess* raw_process = process.get();
  raw_process->ConnectToTracingService(
      std::move(new_connection_request),
      base::BindOnce(&OnProcessConnected, base::Unretained(perfetto_service_),
                     std::move(process), client->pid,
                     std::move(service_receiver)));
}

#if !defined(OS_NACL) && !defined(OS_IOS)
void TracingService::BindConsumerHost(
    mojo::PendingReceiver<mojom::ConsumerHost> receiver) {
  ConsumerHost::BindConsumerReceiver(perfetto_service_, std::move(receiver));
}
#endif

}  // namespace tracing
