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
    mojo::Remote<mojom::TracedProcess> traced_process,
    uint32_t pid,
    mojo::PendingReceiver<mojom::PerfettoService> service_receiver) {
  PerfettoService::GetInstance()->BindReceiver(std::move(service_receiver),
                                               pid);
}

}  // namespace

TracingService::TracingService() = default;

TracingService::TracingService(
    mojo::PendingReceiver<mojom::TracingService> receiver)
    : receiver_(this, std::move(receiver)) {}

TracingService::~TracingService() = default;

void TracingService::Initialize(std::vector<mojom::ClientInfoPtr> clients) {
  for (auto& client : clients) {
    AddClient(std::move(client));
  }
  PerfettoService::GetInstance()->SetActiveServicePidsInitialized();
}

void TracingService::AddClient(mojom::ClientInfoPtr client) {
  PerfettoService::GetInstance()->AddActiveServicePid(client->pid);

  mojo::Remote<mojom::TracedProcess> process(std::move(client->process));
  auto new_connection_request = mojom::ConnectToTracingRequest::New();
  auto service_receiver =
      new_connection_request->perfetto_service.InitWithNewPipeAndPassReceiver();
  mojom::TracedProcess* raw_process = process.get();
  raw_process->ConnectToTracingService(
      std::move(new_connection_request),
      base::BindOnce(&OnProcessConnected, std::move(process), client->pid,
                     std::move(service_receiver)));
}

#if !defined(OS_NACL) && !defined(OS_IOS)
void TracingService::BindConsumerHost(
    mojo::PendingReceiver<mojom::ConsumerHost> receiver) {
  ConsumerHost::BindConsumerReceiver(PerfettoService::GetInstance(),
                                     std::move(receiver));
}
#endif

}  // namespace tracing
