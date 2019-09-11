// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DISCARDS_WEBUI_GRAPH_DUMP_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_DISCARDS_WEBUI_GRAPH_DUMP_IMPL_H_

#include <memory>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/performance_manager/public/graph/frame_node.h"
#include "chrome/browser/performance_manager/public/graph/graph.h"
#include "chrome/browser/performance_manager/public/graph/page_node.h"
#include "chrome/browser/performance_manager/public/graph/process_node.h"
#include "chrome/browser/ui/webui/discards/webui_graph_dump.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace performance_manager {

// TODO(siggi): Add workers to the WebUI graph.
class WebUIGraphDumpImpl : public mojom::WebUIGraphDump,
                           public GraphOwned,
                           public FrameNodeObserver,
                           public PageNodeObserver,
                           public ProcessNodeObserver {
 public:
  WebUIGraphDumpImpl();
  ~WebUIGraphDumpImpl() override;

  // Creates a new WebUIGraphDumpImpl to service |request| and passes its
  // ownership to |graph|.
  static void CreateAndBind(mojom::WebUIGraphDumpRequest request, Graph* graph);

  // Exposed for testing.
  void BindWithGraph(Graph* graph, mojom::WebUIGraphDumpRequest request);

 protected:
  // WebUIGraphDump implementation.
  void SubscribeToChanges(
      mojom::WebUIGraphChangeStreamPtr change_subscriber) override;

  // GraphOwned implementation.
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // FrameNodeObserver implementation:
  void OnFrameNodeAdded(const FrameNode* frame_node) override;
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override;
  // Ignored.
  void OnIsCurrentChanged(const FrameNode* frame_node) override {}
  // Ignored.
  void OnNetworkAlmostIdleChanged(const FrameNode* frame_node) override {}
  // Ignored.
  void OnFrameLifecycleStateChanged(const FrameNode* frame_node) override {}
  // Ignored.
  void OnOriginTrialFreezePolicyChanged(
      const FrameNode* frame_node,
      InterventionPolicy previous_value) override {}
  void OnURLChanged(const FrameNode* frame_node) override;
  // Ignored.
  void OnIsAdFrameChanged(const FrameNode* frame_node) override {}
  void OnNonPersistentNotificationCreated(
      const FrameNode* frame_node) override {}  // Ignored.
  // Ignored.
  void OnPriorityAndReasonChanged(const FrameNode* frame_node) override {}

  // PageNodeObserver implementation:
  void OnPageNodeAdded(const PageNode* page_node) override;
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;
  void OnIsVisibleChanged(const PageNode* page_node) override {}    // Ignored.
  void OnIsAudibleChanged(const PageNode* page_node) override {}    // Ignored.
  void OnIsLoadingChanged(const PageNode* page_node) override {}    // Ignored.
  void OnUkmSourceIdChanged(const PageNode* page_node) override {}  // Ignored.
  // Ignored.
  void OnPageLifecycleStateChanged(const PageNode* page_node) override {}
  // Ignored.
  void OnPageOriginTrialFreezePolicyChanged(
      const PageNode* page_node) override {}
  // Ignored.
  void OnPageAlmostIdleChanged(const PageNode* page_node) override {}
  void OnMainFrameNavigationCommitted(const PageNode* page_node) override;
  void OnTitleUpdated(const PageNode* page_node) override {}  // Ignored.
  void OnFaviconUpdated(const PageNode* page_node) override;

  // ProcessNodeObserver implementation:
  void OnProcessNodeAdded(const ProcessNode* process_node) override;
  void OnProcessLifetimeChange(const ProcessNode* process_node) override;
  void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) override;
  void OnExpectedTaskQueueingDurationSample(
      const ProcessNode* process_node) override {}  // Ignored.
  // Ignored.
  void OnMainThreadTaskLoadIsLow(const ProcessNode* process_node) override {}
  // Ignored.
  void OnAllFramesInProcessFrozen(const ProcessNode* process_node) override {}

 private:
  // The favicon requests happen on the UI thread. This helper class
  // maintains the state required to do that.
  class FaviconRequestHelper;

  FaviconRequestHelper* EnsureFaviconRequestHelper();

  void StartPageFaviconRequest(const PageNode* page_node);
  void StartFrameFaviconRequest(const FrameNode* frame_node);

  void SendFrameNotification(const FrameNode* frame, bool created);
  void SendPageNotification(const PageNode* page, bool created);
  void SendProcessNotification(const ProcessNode* process, bool created);
  void SendDeletionNotification(const Node* node);
  void SendFaviconNotification(
      int64_t serialization_id,
      scoped_refptr<base::RefCountedMemory> bitmap_data);

  static void BindOnPMSequence(mojom::WebUIGraphDumpRequest request,
                               Graph* graph);
  static void OnConnectionError(WebUIGraphDumpImpl* impl);

  Graph* graph_ = nullptr;

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

#endif  // CHROME_BROWSER_UI_WEBUI_DISCARDS_WEBUI_GRAPH_DUMP_IMPL_H_
