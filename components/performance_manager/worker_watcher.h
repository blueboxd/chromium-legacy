// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_WORKER_WATCHER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_WORKER_WATCHER_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "content/public/browser/shared_worker_service.h"

namespace content {
class SharedWorkerInstance;
}

namespace performance_manager {

class FrameNodeImpl;
class FrameNodeSource;
class ProcessNodeSource;
class WorkerNodeImpl;

// This class keeps track of running workers of all types for a single browser
// context and handles the ownership of the worker nodes.
//
// TODO(https://crbug.com/993029): Add support for dedicated workers and service
//                                 workers.
class WorkerWatcher : public content::SharedWorkerService::Observer {
 public:
  WorkerWatcher(const std::string& browser_context_id,
                content::SharedWorkerService* shared_worker_service,
                ProcessNodeSource* process_node_source,
                FrameNodeSource* frame_node_source);
  ~WorkerWatcher() override;

  // Cleans up this instance and ensures shared worker nodes are correctly
  // destroyed on the PM graph.
  void TearDown();

  // content::SharedWorkerService::Observer:
  void OnWorkerStarted(const content::SharedWorkerInstance& instance,
                       int worker_process_id,
                       const base::UnguessableToken& dev_tools_token) override;
  void OnBeforeWorkerTerminated(
      const content::SharedWorkerInstance& instance) override;
  void OnClientAdded(const content::SharedWorkerInstance& instance,
                     int client_process_id,
                     int frame_id) override;
  void OnClientRemoved(const content::SharedWorkerInstance& instance,
                       int client_process_id,
                       int frame_id) override;

 private:
  friend class WorkerWatcherTest;

  void OnBeforeFrameNodeRemoved(int render_process_id,
                                int frame_id,
                                FrameNodeImpl* frame_node);

  bool AddChildWorker(int render_process_id,
                      int frame_id,
                      WorkerNodeImpl* child_worker_node);
  bool RemoveChildWorker(int render_process_id,
                         int frame_id,
                         WorkerNodeImpl* child_worker_node);

  // Helper function to retrieve an existing shared worker node.
  WorkerNodeImpl* GetSharedWorkerNode(
      const content::SharedWorkerInstance& instance);

  // The ID of the BrowserContext who owns the shared worker service.
  const std::string browser_context_id_;

  // Observes the SharedWorkerService for this browser context.
  ScopedObserver<content::SharedWorkerService,
                 content::SharedWorkerService::Observer>
      shared_worker_service_observer_;

  // Used to retrieve an existing process node from its render process ID.
  ProcessNodeSource* const process_node_source_;

  // Used to retrieve an existing frame node from its render process ID and
  // frame ID. Also allows to subscribe to a frame's deletion notification.
  FrameNodeSource* const frame_node_source_;

  // Maps each SharedWorkerInstance to its worker node.
  base::flat_map<content::SharedWorkerInstance, std::unique_ptr<WorkerNodeImpl>>
      shared_worker_nodes_;

  // Maps each frame to the shared workers that this frame is a client of. This
  // is used when a frame is torn down before the OnBeforeWorkerTerminated() is
  // received, to ensure the deletion of the worker nodes in the right order
  // (workers before frames).
  struct FrameInfo {
    int render_process_id;
    int frame_id;
  };

  // Comparison operator to allow using FrameInfo as a key.
  friend bool operator<(const FrameInfo& lhs, const FrameInfo& rhs);

  base::flat_map<FrameInfo, base::flat_set<WorkerNodeImpl*>>
      frame_node_child_workers_;

#if DCHECK_IS_ON()
  // Keeps track of how many OnClientRemoved() calls are expected for an
  // existing worker. This happens when OnBeforeFrameNodeRemoved() is invoked
  // before OnClientRemoved().
  base::flat_map<WorkerNodeImpl*, int> clients_to_remove_;
#endif  // DCHECK_IS_ON()

  DISALLOW_COPY_AND_ASSIGN(WorkerWatcher);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_WORKER_WATCHER_H_
