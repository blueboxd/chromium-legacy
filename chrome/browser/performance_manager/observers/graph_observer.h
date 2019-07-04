// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_GRAPH_OBSERVER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_GRAPH_OBSERVER_H_

#include "base/macros.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"

namespace performance_manager {

class FrameNodeImpl;
class GraphImpl;
class NodeBase;
class PageNodeImpl;
class ProcessNodeImpl;
class SystemNodeImpl;

// An observer API for the graph.
//
// Observers are generally instantiated when the graph is empty, and outlive it,
// though it's valid for an observer to be registered at any time. Observers
// must unregister before they're destroyed.
//
// To create and install a new observer:
//   (1) Derive from this class.
//   (2) Register by calling on |graph().RegisterObserver|.
//   (3) Before destruction, unregister by calling on
//       |graph().UnregisterObserver|.
//
// NOTE: This interface is deprecated. Please use the public observer interfaces
// directly!
class GraphImplObserver {
 public:
  GraphImplObserver();
  virtual ~GraphImplObserver();

  // Determines whether or not the observer should be registered with, and
  // invoked for, the |node|.
  // TODO(chrisha): Kill this function entirely and delegate that logic to the
  // actual observer implementations.
  virtual bool ShouldObserve(const NodeBase* node) = 0;

  // Invoked when an observer is added to or removed from the graph. This is a
  // convenient place for observers to initialize any necessary state, validate
  // graph invariants, etc.
  virtual void OnRegistered() = 0;
  virtual void OnUnregistered() = 0;

  // Called whenever a node has been added to the graph.
  virtual void OnNodeAdded(NodeBase* node) = 0;

  // Called when the |node| is about to be removed from the graph.
  virtual void OnBeforeNodeRemoved(NodeBase* node) = 0;

  // This will be called with a non-null |graph| when the observer is attached
  // to a graph, and then again with a null |graph| when the observer is
  // removed.
  virtual void SetGraph(GraphImpl* graph) = 0;

  // FrameNodeObserver analogs:
  virtual void OnIsCurrentChanged(FrameNodeImpl* frame_node) = 0;
  virtual void OnNetworkAlmostIdleChanged(FrameNodeImpl* frame_node) = 0;
  virtual void OnLifecycleStateChanged(FrameNodeImpl* frame_node) = 0;
  virtual void OnURLChanged(FrameNodeImpl* frame_node) = 0;
  virtual void OnNonPersistentNotificationCreated(
      FrameNodeImpl* frame_node) = 0;

  // PageNodeObserver analogs:
  virtual void OnIsVisibleChanged(PageNodeImpl* page_node) = 0;
  virtual void OnIsAudibleChanged(PageNodeImpl* page_node) = 0;
  virtual void OnIsLoadingChanged(PageNodeImpl* page_node) = 0;
  virtual void OnUkmSourceIdChanged(PageNodeImpl* page_node) = 0;
  virtual void OnLifecycleStateChanged(PageNodeImpl* page_node) = 0;
  virtual void OnPageAlmostIdleChanged(PageNodeImpl* page_node) = 0;
  virtual void OnFaviconUpdated(PageNodeImpl* page_node) = 0;
  virtual void OnTitleUpdated(PageNodeImpl* page_node) = 0;
  virtual void OnMainFrameNavigationCommitted(PageNodeImpl* page_node) = 0;

  // ProcessNodeObserver analogs:
  virtual void OnExpectedTaskQueueingDurationSample(
      ProcessNodeImpl* process_node) = 0;
  virtual void OnMainThreadTaskLoadIsLow(ProcessNodeImpl* process_node) = 0;
  virtual void OnAllFramesInProcessFrozen(ProcessNodeImpl* process_node) = 0;

  // SystemNodeObserver analogs:
  virtual void OnProcessCPUUsageReady(SystemNodeImpl* system_node) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(GraphImplObserver);
};

// An empty implementation of the interface.
class GraphImplObserverDefaultImpl : public GraphImplObserver {
 public:
  GraphImplObserverDefaultImpl();
  ~GraphImplObserverDefaultImpl() override;

  // GraphObserver implementation:
  void OnRegistered() override {}
  void OnUnregistered() override {}
  void OnNodeAdded(NodeBase* node) override {}
  void OnBeforeNodeRemoved(NodeBase* node) override {}
  void SetGraph(GraphImpl* graph) override;

  // FrameNodeImplObserver implementation:
  void OnIsCurrentChanged(FrameNodeImpl* frame_node) override {}
  void OnNetworkAlmostIdleChanged(FrameNodeImpl* frame_node) override {}
  void OnLifecycleStateChanged(FrameNodeImpl* frame_node) override {}
  void OnURLChanged(FrameNodeImpl* frame_node) override {}
  void OnNonPersistentNotificationCreated(FrameNodeImpl* frame_node) override {}

  // PageNodeImplObserver implementation:
  void OnIsVisibleChanged(PageNodeImpl* page_node) override {}
  void OnIsAudibleChanged(PageNodeImpl* page_node) override {}
  void OnIsLoadingChanged(PageNodeImpl* page_node) override {}
  void OnUkmSourceIdChanged(PageNodeImpl* page_node) override {}
  void OnLifecycleStateChanged(PageNodeImpl* page_node) override {}
  void OnPageAlmostIdleChanged(PageNodeImpl* page_node) override {}
  void OnFaviconUpdated(PageNodeImpl* page_node) override {}
  void OnTitleUpdated(PageNodeImpl* page_node) override {}
  void OnMainFrameNavigationCommitted(PageNodeImpl* page_node) override {}

  // ProcessNodeImplObserver implementation:
  void OnExpectedTaskQueueingDurationSample(
      ProcessNodeImpl* process_node) override {}
  void OnMainThreadTaskLoadIsLow(ProcessNodeImpl* process_node) override {}
  void OnAllFramesInProcessFrozen(ProcessNodeImpl* process_node) override {}

  // SystemNodeImplObserver implementation:
  void OnProcessCPUUsageReady(SystemNodeImpl* system_node) override {}

  GraphImpl* graph() const { return graph_; }

 private:
  GraphImpl* graph_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(GraphImplObserverDefaultImpl);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_GRAPH_OBSERVER_H_
