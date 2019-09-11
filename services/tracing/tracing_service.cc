// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/tracing_service.h"

#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/timer/timer.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"
#include "services/tracing/agent_registry.h"
#include "services/tracing/coordinator.h"
#include "services/tracing/perfetto/consumer_host.h"
#include "services/tracing/perfetto/perfetto_service.h"
#include "services/tracing/perfetto/perfetto_tracing_coordinator.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "services/tracing/public/mojom/traced_process.mojom.h"

namespace tracing {

// Listener used to connect to every other service and
// pass them the needed interface pointers to connect
// back and register with the tracing service.
class ServiceListener : public service_manager::mojom::ServiceManagerListener {
 public:
  ServiceListener(service_manager::Connector* connector,
                  AgentRegistry* agent_registry,
                  Coordinator* coordinator)
      : connector_(connector),
        agent_registry_(agent_registry),
        coordinator_(coordinator) {
    service_manager::mojom::ServiceManagerPtr service_manager;
    connector_->BindInterface(service_manager::mojom::kServiceName,
                              &service_manager);
    service_manager::mojom::ServiceManagerListenerPtr listener;
    service_manager::mojom::ServiceManagerListenerRequest request(
        mojo::MakeRequest(&listener));
    service_manager->AddListener(std::move(listener));
    binding_.Bind(std::move(request));
  }

  size_t CountServicesWithPID(uint32_t pid) {
    return std::count_if(service_pid_map_.begin(), service_pid_map_.end(),
                         [pid](const auto& p) { return p.second == pid; });
  }

  void ServiceAddedWithPID(const service_manager::Identity& identity,
                           uint32_t pid) {
    service_pid_map_[identity] = pid;

    // Not the first service added for this PID, and the process has already
    // accepted a connection request.
    if (base::Contains(connected_pids_, pid))
      return;

    // Let the Coordinator and the perfetto service know it should be expecting
    // a connection from this process.
    coordinator_->AddExpectedPID(pid);
    PerfettoService::GetInstance()->AddActiveServicePid(pid);

    // NOTE: If multiple service instances are running in the same process, we
    // may send multiple ConnectToTracingService calls to the same process in
    // the time it takes the first call to be received and acknowledged. This is
    // OK, because any given client process will only bind a single
    // TracedProcess endpoint as long as this instance of the tracing service
    // remains alive. Subsequent TracedProcess endpoints will be dropped and
    // their calls will never be processed.

    mojom::TracedProcessPtr traced_process;
    connector_->BindInterface(
        service_manager::ServiceFilter::ForExactIdentity(identity),
        mojo::MakeRequest(&traced_process),
        service_manager::mojom::BindInterfacePriority::kBestEffort);

    auto new_connection_request = mojom::ConnectToTracingRequest::New();
    auto service_request =
        mojo::MakeRequest(&new_connection_request->perfetto_service);
    auto registry_request =
        mojo::MakeRequest(&new_connection_request->agent_registry);
    mojom::TracedProcess* raw_traced_process = traced_process.get();
    raw_traced_process->ConnectToTracingService(
        std::move(new_connection_request),
        base::BindOnce(&ServiceListener::OnProcessConnected,
                       base::Unretained(this), std::move(traced_process), pid,
                       std::move(service_request),
                       std::move(registry_request)));
  }

  void ServiceRemoved(const service_manager::Identity& identity) {
    auto entry = service_pid_map_.find(identity);
    if (entry != service_pid_map_.end()) {
      uint32_t pid = entry->second;
      service_pid_map_.erase(entry);
      // Last entry with this PID removed; stop expecting it
      // to connect to the tracing service.
      if (CountServicesWithPID(pid) == 0) {
        coordinator_->RemoveExpectedPID(pid);
        PerfettoService::GetInstance()->RemoveActiveServicePid(pid);
        connected_pids_.erase(pid);
      }
    }
  }

  // service_manager::mojom::ServiceManagerListener implementation.
  void OnInit(std::vector<service_manager::mojom::RunningServiceInfoPtr>
                  running_services) override {
    for (auto& service : running_services) {
      if (service->pid) {
        ServiceAddedWithPID(service->identity, service->pid);
      }
    }

    coordinator_->FinishedReceivingRunningPIDs();
    PerfettoService::GetInstance()->SetActiveServicePidsInitialized();
  }

  void OnServicePIDReceived(const service_manager::Identity& identity,
                            uint32_t pid) override {
    ServiceAddedWithPID(identity, pid);
  }

  void OnServiceFailedToStart(
      const service_manager::Identity& identity) override {
    ServiceRemoved(identity);
  }

  void OnServiceStopped(const service_manager::Identity& identity) override {
    ServiceRemoved(identity);
  }

  void OnServiceStarted(const service_manager::Identity& identity,
                        uint32_t pid) override {
  }

  void OnServiceCreated(
      service_manager::mojom::RunningServiceInfoPtr service) override {}

 private:
  void OnProcessConnected(
      mojom::TracedProcessPtr traced_process,
      uint32_t pid,
      mojom::PerfettoServiceRequest service_request,
      mojo::PendingReceiver<mojom::AgentRegistry> registry_receiver) {
    auto result = connected_pids_.insert(pid);
    if (!result.second) {
      // The PID was already connected. Nothing more to do.
      return;
    }

    connected_pids_.insert(pid);
    PerfettoService::GetInstance()->BindRequest(std::move(service_request),
                                                pid);
    agent_registry_->BindAgentRegistryReceiver(std::move(registry_receiver));
  }

  mojo::Binding<service_manager::mojom::ServiceManagerListener> binding_{this};
  service_manager::Connector* const connector_;
  AgentRegistry* const agent_registry_;
  Coordinator* const coordinator_;
  std::map<service_manager::Identity, uint32_t> service_pid_map_;
  std::set<uint32_t> connected_pids_;

  DISALLOW_COPY_AND_ASSIGN(ServiceListener);
};

TracingService::TracingService(service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)) {}

TracingService::~TracingService() = default;

void TracingService::OnDisconnected() {
  CloseAgentConnectionsAndTerminate();
}

void TracingService::OnStart() {
  tracing_agent_registry_ = std::make_unique<AgentRegistry>();

  if (TracingUsesPerfettoBackend()) {
    auto perfetto_coordinator = std::make_unique<PerfettoTracingCoordinator>(
        tracing_agent_registry_.get(),
        base::BindRepeating(&TracingService::OnCoordinatorConnectionClosed,
                            base::Unretained(this)));
    registry_.AddInterface(
        base::BindRepeating(&PerfettoTracingCoordinator::BindCoordinatorRequest,
                            base::Unretained(perfetto_coordinator.get())));
    tracing_coordinator_ = std::move(perfetto_coordinator);
  } else {
    auto tracing_coordinator = std::make_unique<Coordinator>(
        tracing_agent_registry_.get(),
        base::BindRepeating(&TracingService::OnCoordinatorConnectionClosed,
                            base::Unretained(this)));
    registry_.AddInterface(
        base::BindRepeating(&Coordinator::BindCoordinatorRequest,
                            base::Unretained(tracing_coordinator.get())));
    tracing_coordinator_ = std::move(tracing_coordinator);
  }

  registry_.AddInterface(
      base::BindRepeating(&ConsumerHost::BindConsumerRequest,
                          base::Unretained(PerfettoService::GetInstance())));

  service_listener_ = std::make_unique<ServiceListener>(
      service_binding_.GetConnector(), tracing_agent_registry_.get(),
      tracing_coordinator_.get());
}

void TracingService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe),
                          source_info);
}

void TracingService::OnCoordinatorConnectionClosed() {
  service_binding_.RequestClose();
}

void TracingService::CloseAgentConnectionsAndTerminate() {
  tracing_agent_registry_->DisconnectAllAgents();
  Terminate();
}

}  // namespace tracing
