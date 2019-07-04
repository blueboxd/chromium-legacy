// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/location.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/performance_manager/graph/graph_impl.h"
#include "chrome/browser/performance_manager/performance_manager.h"
#include "chrome/browser/performance_manager/public/web_contents_proxy.h"
#include "chrome/browser/performance_manager/webui_graph_dump_impl.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"

namespace ukm {
class MojoUkmRecorder;
}  // namespace ukm

namespace performance_manager {

class PageNodeImpl;

// The performance manager is a rendezvous point for binding to performance
// manager interfaces.
// TODO(https://crbug.com/910288): Refactor this along with the
// {Frame|Page|Process|System}ResourceCoordinator classes.
class PerformanceManager {
 public:
  using FrameNodeCreationCallback = base::OnceCallback<void(FrameNodeImpl*)>;

  ~PerformanceManager();

  // Retrieves the currently registered instance.
  // The caller needs to ensure that the lifetime of the registered instance
  // exceeds the use of this function and the retrieved pointer.
  // This function can be called from any sequence with those caveats.
  static PerformanceManager* GetInstance();

  // Creates, initializes and registers an instance.
  static std::unique_ptr<PerformanceManager> Create();

  // Unregisters |instance| if it's currently registered and arranges for its
  // deletion on its sequence.
  static void Destroy(std::unique_ptr<PerformanceManager> instance);

  // Invokes |graph_callback| on the performance manager's sequence, with the
  // graph as a parameter.
  using GraphCallback = base::OnceCallback<void(GraphImpl*)>;
  void CallOnGraph(const base::Location& from_here,
                   GraphCallback graph_callback);

  // Forwards the binding request to the implementation class.
  template <typename Interface>
  void BindInterface(mojo::InterfaceRequest<Interface> request);

  // Creates a new node of the requested type and adds it to the graph.
  // May be called from any sequence. If a |creation_callback| is provided it
  // will be run on the performance manager sequence immediately after creating
  // the node.
  std::unique_ptr<FrameNodeImpl> CreateFrameNode(
      ProcessNodeImpl* process_node,
      PageNodeImpl* page_node,
      FrameNodeImpl* parent_frame_node,
      int frame_tree_node_id,
      const base::UnguessableToken& dev_tools_token,
      int32_t browsing_instance_id,
      int32_t site_instance_id);
  std::unique_ptr<FrameNodeImpl> CreateFrameNode(
      ProcessNodeImpl* process_node,
      PageNodeImpl* page_node,
      FrameNodeImpl* parent_frame_node,
      int frame_tree_node_id,
      const base::UnguessableToken& dev_tools_token,
      int32_t browsing_instance_id,
      int32_t site_instance_id,
      FrameNodeCreationCallback creation_callback);
  std::unique_ptr<PageNodeImpl> CreatePageNode(
      const WebContentsProxy& contents_proxy,
      bool is_visible,
      bool is_audible);
  std::unique_ptr<ProcessNodeImpl> CreateProcessNode();

  // Destroys a node returned from the creation functions above.
  // May be called from any sequence.
  template <typename NodeType>
  void DeleteNode(std::unique_ptr<NodeType> node);

  // Each node in |nodes| must have been returned from one of the creation
  // functions above. This function takes care of removing them from the graph
  // in topological order and destroying them.
  void BatchDeleteNodes(std::vector<std::unique_ptr<NodeBase>> nodes);

  // TODO(siggi): Can this be hidden away?
  scoped_refptr<base::SequencedTaskRunner> task_runner() const {
    return task_runner_;
  }

  // This allows an observer to be passed to this class, and bound to the
  // lifetime of the performance manager. This will disappear post
  // resource_coordinator migration, so do not use this unless you know what
  // you're doing! Must be called from the performance manager sequence.
  // TODO(chrisha): Kill this dead.
  void RegisterObserver(std::unique_ptr<GraphImplObserver> observer);

 private:
  using InterfaceRegistry = service_manager::BinderRegistryWithArgs<
      const service_manager::BindSourceInfo&>;

  PerformanceManager();
  void PostBindInterface(const std::string& interface_name,
                         mojo::ScopedMessagePipeHandle message_pipe);

  template <typename NodeType, typename... Args>
  std::unique_ptr<NodeType> CreateNodeImpl(
      base::OnceCallback<void(NodeType*)> creation_callback,
      Args&&... constructor_args);

  void PostDeleteNode(std::unique_ptr<NodeBase> node);
  void DeleteNodeImpl(std::unique_ptr<NodeBase> node);
  void BatchDeleteNodesImpl(std::vector<std::unique_ptr<NodeBase>> nodes);

  void OnStart();
  void OnStartImpl(std::unique_ptr<service_manager::Connector> connector);
  void CallOnGraphImpl(GraphCallback graph_callback);
  void BindInterfaceImpl(const std::string& interface_name,
                         mojo::ScopedMessagePipeHandle message_pipe);

  void BindWebUIGraphDump(mojom::WebUIGraphDumpRequest request,
                          const service_manager::BindSourceInfo& source_info);
  void OnGraphDumpConnectionError(WebUIGraphDumpImpl* graph_dump);

  InterfaceRegistry interface_registry_;

  // The performance task runner.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  GraphImpl graph_;

  // The registered graph observers.
  std::vector<std::unique_ptr<GraphImplObserver>> observers_;

  // Provided to |graph_|.
  // TODO(siggi): This no longer needs to go through mojo.
  std::unique_ptr<ukm::MojoUkmRecorder> ukm_recorder_;

  // Current graph dump instances.
  std::vector<std::unique_ptr<WebUIGraphDumpImpl>> graph_dumps_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PerformanceManager);
};

template <typename Interface>
void PerformanceManager::BindInterface(
    mojo::InterfaceRequest<Interface> request) {
  PostBindInterface(Interface::Name_, request.PassMessagePipe());
}

template <typename NodeType>
void PerformanceManager::DeleteNode(std::unique_ptr<NodeType> node) {
  PostDeleteNode(std::move(node));
}

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_H_
