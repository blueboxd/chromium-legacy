// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/agent_registry.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/threading/thread_checker.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace tracing {

const int32_t kAgentResponseTimeoutInSeconds = 10;

AgentRegistry::AgentEntry::AgentEntry(size_t id,
                                      AgentRegistry* agent_registry,
                                      mojo::PendingRemote<mojom::Agent> agent,
                                      const std::string& label,
                                      mojom::TraceDataType type,
                                      base::ProcessId pid)
    : id_(id),
      agent_registry_(agent_registry),
      agent_(std::move(agent)),
      label_(label),
      type_(type),
      pid_(pid) {
  DCHECK(!label.empty());
  agent_.set_disconnect_handler(base::BindRepeating(
      &AgentRegistry::AgentEntry::OnConnectionError, AsWeakPtr()));
}

AgentRegistry::AgentEntry::~AgentEntry() = default;

void AgentRegistry::AgentEntry::AddDisconnectClosure(
    const void* closure_name,
    base::OnceClosure closure) {
  DCHECK_EQ(0u, closures_.count(closure_name));
  closures_[closure_name] = std::move(closure);

  // Adding a disconnect closure means we're waiting for a response from the
  // agent. If the client becomes unresponsive, we disconnect it.
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromSeconds(kAgentResponseTimeoutInSeconds),
               this, &AgentRegistry::AgentEntry::OnConnectionError);
}

bool AgentRegistry::AgentEntry::RemoveDisconnectClosure(
    const void* closure_name) {
  timer_.Stop();
  return closures_.erase(closure_name) > 0;
}

bool AgentRegistry::AgentEntry::HasDisconnectClosure(const void* closure_name) {
  return closures_.count(closure_name) > 0;
}

void AgentRegistry::AgentEntry::OnConnectionError() {
  // Run disconnect closures if there is any. We should mark |key_value.second|
  // as movable so that the version of |Run| that takes an rvalue reference is
  // selected not the version that takes a const reference. The former is for
  // once callbacks and the latter is for repeating callbacks.
  while (!closures_.empty()) {
    auto iterator = closures_.begin();
    auto callback = std::move(iterator->second);
    const size_t closures_size = closures_.size();
    std::move(callback).Run();
    // Verify that the callback has removed itself.
    DCHECK_EQ(1u, closures_size - closures_.size());
  }
  agent_registry_->UnregisterAgent(id_);
}

AgentRegistry::AgentRegistry() = default;

AgentRegistry::~AgentRegistry() = default;

void AgentRegistry::DisconnectAllAgents() {
  receivers_.Clear();
}

void AgentRegistry::BindAgentRegistryReceiver(
    mojo::PendingReceiver<mojom::AgentRegistry> receiver) {
  receivers_.Add(this, std::move(receiver));
}

size_t AgentRegistry::SetAgentInitializationCallback(
    const AgentInitializationCallback& callback,
    bool call_on_new_agents_only) {
  agent_initialization_callback_ = callback;
  size_t num_initialized_agents = 0;
  if (!call_on_new_agents_only) {
    ForAllAgents([this, &num_initialized_agents](AgentEntry* agent_entry) {
      agent_initialization_callback_.Run(agent_entry);
      num_initialized_agents++;
    });
  }
  return num_initialized_agents;
}

bool AgentRegistry::HasDisconnectClosure(const void* closure_name) {
  for (const auto& key_value : agents_) {
    if (key_value.second->HasDisconnectClosure(closure_name))
      return true;
  }
  return false;
}

void AgentRegistry::RegisterAgent(mojo::PendingRemote<mojom::Agent> agent,
                                  const std::string& label,
                                  mojom::TraceDataType type,
                                  base::ProcessId pid) {
  auto id = next_agent_id_++;
  auto entry = std::make_unique<AgentEntry>(id, this, std::move(agent), label,
                                            type, pid);
  AgentEntry* entry_ptr = entry.get();
  auto result = agents_.insert(std::make_pair(id, std::move(entry)));
  DCHECK(result.second);
  if (!agent_initialization_callback_.is_null())
    agent_initialization_callback_.Run(entry_ptr);
}

void AgentRegistry::UnregisterAgent(size_t agent_id) {
  size_t num_deleted = agents_.erase(agent_id);
  DCHECK_EQ(1u, num_deleted);
}

}  // namespace tracing
