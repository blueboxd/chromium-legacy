// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_WEBUI_GRAPH_DUMP_IMPL_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_WEBUI_GRAPH_DUMP_IMPL_H_

#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/performance_manager/observers/graph_observer.h"
#include "chrome/browser/performance_manager/webui_graph_dump.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace performance_manager {

class GraphImpl;

class WebUIGraphDumpImpl : public mojom::WebUIGraphDump,
                           public GraphImplObserver {
 public:
  explicit WebUIGraphDumpImpl(GraphImpl* graph);
  ~WebUIGraphDumpImpl() override;

  // Bind this instance to |request| with the |error_handler|.
  void Bind(mojom::WebUIGraphDumpRequest request,
            base::OnceClosure error_handler);

  // WebUIGraphDump implementation.
  void SubscribeToChanges(
      mojom::WebUIGraphChangeStreamPtr change_subscriber) override;

  void OnRegistered() override {}
  void OnUnregistered() override {}
  bool ShouldObserve(const NodeBase* node) override;
  void OnNodeAdded(NodeBase* node) override;
  void OnBeforeNodeRemoved(NodeBase* node) override;
  void SetGraph(GraphImpl* graph) override;

  // Frame node functions.
  void OnIsCurrentChanged(FrameNodeImpl* frame_node) override;
  void OnNetworkAlmostIdleChanged(FrameNodeImpl* frame_node) override;
  void OnLifecycleStateChanged(FrameNodeImpl* frame_node) override;
  void OnURLChanged(FrameNodeImpl* frame_node) override;
  // Event notification.
  void OnNonPersistentNotificationCreated(FrameNodeImpl* frame_node) override {}

  // Page node functions.
  void OnIsVisibleChanged(PageNodeImpl* page_node) override;
  void OnIsAudibleChanged(PageNodeImpl* page_node) override;
  void OnIsLoadingChanged(PageNodeImpl* page_node) override;
  void OnUkmSourceIdChanged(PageNodeImpl* page_node) override;
  void OnLifecycleStateChanged(PageNodeImpl* page_node) override;
  void OnPageAlmostIdleChanged(PageNodeImpl* page_node) override;
  // Event notification.
  void OnFaviconUpdated(PageNodeImpl* page_node) override;
  // Event notification.
  void OnTitleUpdated(PageNodeImpl* page_node) override {}
  // Event notification that also implies the main_frame_url changed.
  void OnMainFrameNavigationCommitted(PageNodeImpl* page_node) override;

  // Process node functions.
  void OnExpectedTaskQueueingDurationSample(
      ProcessNodeImpl* process_node) override;
  void OnMainThreadTaskLoadIsLow(ProcessNodeImpl* process_node) override;
  // Event notification.
  void OnAllFramesInProcessFrozen(ProcessNodeImpl* process_node) override {}

  // System node functions.
  // Ignored.
  void OnProcessCPUUsageReady(SystemNodeImpl* system_node) override {}

 private:
  // The favicon requests happen on the UI thread. This helper class
  // maintains the state required to do that.
  class FaviconRequestHelper;

  FaviconRequestHelper* EnsureFaviconRequestHelper();

  void StartPageFaviconRequest(PageNodeImpl* page_node);
  void StartFrameFaviconRequest(FrameNodeImpl* frame_node);

  void SendFrameNotification(FrameNodeImpl* frame, bool created);
  void SendPageNotification(PageNodeImpl* page, bool created);
  void SendProcessNotification(ProcessNodeImpl* process, bool created);
  void SendDeletionNotification(NodeBase* node);
  void SendFaviconNotification(
      int64_t serialization_id,
      scoped_refptr<base::RefCountedMemory> bitmap_data);

  GraphImpl* graph_;

  std::unique_ptr<FaviconRequestHelper> favicon_request_helper_;

  // The current change subscriber to this dumper. This instance is subscribed
  // to every node in |graph_| save for the system node, so long as there is a
  // subscriber.
  mojom::WebUIGraphChangeStreamPtr change_subscriber_;
  mojo::Binding<mojom::WebUIGraphDump> binding_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WebUIGraphDumpImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebUIGraphDumpImpl);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_WEBUI_GRAPH_DUMP_IMPL_H_
