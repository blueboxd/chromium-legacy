// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_AGENT_REGISTRY_H_
#define SERVICES_TRACING_AGENT_REGISTRY_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/tracing/public/mojom/tracing.mojom.h"

namespace tracing {

class AgentRegistry : public mojom::AgentRegistry {
 public:
  class AgentEntry : public base::SupportsWeakPtr<AgentEntry> {
   public:
    AgentEntry(size_t id,
               AgentRegistry* agent_registry,
               mojo::PendingRemote<mojom::Agent> agent,
               const std::string& label,
               mojom::TraceDataType type,
               base::ProcessId pid);
    ~AgentEntry();

    void AddDisconnectClosure(const void* closure_name,
                              base::OnceClosure closure);
    bool RemoveDisconnectClosure(const void* closure_name);
    bool HasDisconnectClosure(const void* closure_name);
    size_t num_disconnect_closures_for_testing() const {
      return closures_.size();
    }

    mojom::Agent* agent() const { return agent_.get(); }
    const std::string& label() const { return label_; }
    mojom::TraceDataType type() const { return type_; }
    base::ProcessId pid() const { return pid_; }

   private:
    void OnConnectionError();

    const size_t id_;
    AgentRegistry* agent_registry_;
    mojo::Remote<mojom::Agent> agent_;
    const std::string label_;
    const mojom::TraceDataType type_;
    const base::ProcessId pid_;
    std::map<const void*, base::OnceClosure> closures_;
    base::RepeatingTimer timer_;

    DISALLOW_COPY_AND_ASSIGN(AgentEntry);
  };

  // A function to be run for every agent that registers itself.
  using AgentInitializationCallback =
      base::RepeatingCallback<void(AgentEntry*)>;

  AgentRegistry();
  ~AgentRegistry() override;

  void DisconnectAllAgents();

  void BindAgentRegistryReceiver(
      mojo::PendingReceiver<mojom::AgentRegistry> receiver);

  // Returns the number of existing agents that the callback was run on.
  size_t SetAgentInitializationCallback(
      const AgentInitializationCallback& callback,
      bool call_on_new_agents_only);
  bool HasDisconnectClosure(const void* closure_name);

  template <typename FunctionType>
  void ForAllAgents(FunctionType function) {
    for (const auto& key_value : agents_) {
      function(key_value.second.get());
    }
  }

 private:
  friend class AgentRegistryTest;  // For testing.
  friend class CoordinatorTestUtil;  // For testing.

  // mojom::AgentRegistry
  void RegisterAgent(mojo::PendingRemote<mojom::Agent> agent,
                     const std::string& label,
                     mojom::TraceDataType type,
                     base::ProcessId pid) override;

  void UnregisterAgent(size_t agent_id);

  mojo::ReceiverSet<mojom::AgentRegistry> receivers_;
  size_t next_agent_id_ = 0;
  std::map<size_t, std::unique_ptr<AgentEntry>> agents_;
  AgentInitializationCallback agent_initialization_callback_;

  DISALLOW_COPY_AND_ASSIGN(AgentRegistry);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_AGENT_REGISTRY_H_
