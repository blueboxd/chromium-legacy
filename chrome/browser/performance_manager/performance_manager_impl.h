// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_IMPL_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_IMPL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "chrome/browser/performance_manager/graph/graph_impl.h"
#include "chrome/browser/performance_manager/public/graph/worker_node.h"
#include "chrome/browser/performance_manager/public/performance_manager.h"
#include "chrome/browser/performance_manager/public/render_process_host_proxy.h"
#include "chrome/browser/performance_manager/public/web_contents_proxy.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"

class GURL;

namespace ukm {
class MojoUkmRecorder;
}  // namespace ukm

namespace performance_manager {

class PageNodeImpl;

// The performance manager is a rendezvous point for binding to performance
// manager interfaces.
// TODO(https://crbug.com/910288): Refactor this along with the
// {Frame|Page|Process|System}ResourceCoordinator classes.
class PerformanceManagerImpl : public PerformanceManager {
 public:
  using FrameNodeCreationCallback = base::OnceCallback<void(FrameNodeImpl*)>;

  ~PerformanceManagerImpl() override;

  // Posts a callback that will run on the PM sequence, and be provided a
  // pointer to the Graph. Valid to be called from the main thread only, and
  // only if "IsAvailable" returns true.
  // TODO(chrisha): Move this to the public interface.
  using GraphImplCallback = base::OnceCallback<void(GraphImpl*)>;
  static void CallOnGraphImpl(const base::Location& from_here,
                              GraphImplCallback graph_callback);

  // Posts a callback that will run on the PM sequence, and be provided a
  // pointer to the Graph. Valid to be called from the main thread only, and
  // only if "IsAvailable" returns true. The return value is returned as an
  // argument to the reply callback.
  template <typename TaskReturnType>
  void CallOnGraphAndReplyWithResult(
      const base::Location& from_here,
      base::OnceCallback<TaskReturnType(GraphImpl*)> task,
      base::OnceCallback<void(TaskReturnType)> reply);

  // Retrieves the currently registered instance.
  // The caller needs to ensure that the lifetime of the registered instance
  // exceeds the use of this function and the retrieved pointer.
  // This function can be called from any sequence with those caveats.
  static PerformanceManagerImpl* GetInstance();

  // Creates, initializes and registers an instance.
  static std::unique_ptr<PerformanceManagerImpl> Create();

  // Unregisters |instance| if it's currently registered and arranges for its
  // deletion on its sequence.
  static void Destroy(std::unique_ptr<PerformanceManagerImpl> instance);

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
      const std::string& browser_context_id,
      bool is_visible,
      bool is_audible);
  std::unique_ptr<ProcessNodeImpl> CreateProcessNode(
      RenderProcessHostProxy proxy);
  std::unique_ptr<WorkerNodeImpl> CreateWorkerNode(
      const std::string& browser_context_id,
      WorkerNode::WorkerType worker_type,
      ProcessNodeImpl* process_node,
      const GURL& url,
      const base::UnguessableToken& dev_tools_token);

  // Destroys a node returned from the creation functions above.
  // May be called from any sequence.
  template <typename NodeType>
  void DeleteNode(std::unique_ptr<NodeType> node);

  // Each node in |nodes| must have been returned from one of the creation
  // functions above. This function takes care of removing them from the graph
  // in topological order and destroying them.
  void BatchDeleteNodes(std::vector<std::unique_ptr<NodeBase>> nodes);

  // TODO(chrisha): Hide this after the last consumer stops using it!
  scoped_refptr<base::SequencedTaskRunner> task_runner() const {
    return task_runner_;
  }

  // Indicates whether or not the caller is currently running on the PM task
  // runner.
  bool OnPMTaskRunnerForTesting() const {
    return task_runner_->RunsTasksInCurrentSequence();
  }

 private:
  friend class PerformanceManager;

  PerformanceManagerImpl();

  template <typename NodeType, typename... Args>
  std::unique_ptr<NodeType> CreateNodeImpl(
      base::OnceCallback<void(NodeType*)> creation_callback,
      Args&&... constructor_args);

  void PostDeleteNode(std::unique_ptr<NodeBase> node);
  void DeleteNodeImpl(std::unique_ptr<NodeBase> node);
  void BatchDeleteNodesImpl(std::vector<std::unique_ptr<NodeBase>> nodes);

  void OnStart();
  void OnStartImpl(std::unique_ptr<service_manager::Connector> connector);
  void RunCallbackWithGraphImpl(GraphImplCallback graph_callback);
  void RunCallbackWithGraph(GraphCallback graph_callback);

  template <typename TaskReturnType>
  TaskReturnType RunCallbackWithGraphAndReplyWithResult(
      base::OnceCallback<TaskReturnType(GraphImpl*)> task);

  // The performance task runner.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  GraphImpl graph_;

  // Provided to |graph_|.
  // TODO(siggi): This no longer needs to go through mojo.
  std::unique_ptr<ukm::MojoUkmRecorder> ukm_recorder_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PerformanceManagerImpl);
};

template <typename NodeType>
void PerformanceManagerImpl::DeleteNode(std::unique_ptr<NodeType> node) {
  PostDeleteNode(std::move(node));
}

template <typename TaskReturnType>
void PerformanceManagerImpl::CallOnGraphAndReplyWithResult(
    const base::Location& from_here,
    base::OnceCallback<TaskReturnType(GraphImpl*)> task,
    base::OnceCallback<void(TaskReturnType)> reply) {
  auto* pm = GetInstance();
  base::PostTaskAndReplyWithResult(
      pm->task_runner_.get(), from_here,
      base::BindOnce(
          &PerformanceManagerImpl::RunCallbackWithGraphAndReplyWithResult<
              TaskReturnType>,
          base::Unretained(pm), std::move(task)),
      std::move(reply));
}

template <typename TaskReturnType>
TaskReturnType PerformanceManagerImpl::RunCallbackWithGraphAndReplyWithResult(
    base::OnceCallback<TaskReturnType(GraphImpl*)> task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return std::move(task).Run(&graph_);
}

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_IMPL_H_
