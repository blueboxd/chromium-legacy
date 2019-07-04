// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/graph_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/node_base.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/graph/system_node_impl.h"
#include "chrome/browser/performance_manager/observers/graph_observer.h"

namespace ukm {
class UkmEntryBuilder;
}  // namespace ukm

namespace performance_manager {

namespace {

// A unique type ID for this implementation.
const uintptr_t kGraphImplType = reinterpret_cast<uintptr_t>(&kGraphImplType);

template <typename NodeObserverType>
void AddObserverImpl(std::vector<NodeObserverType*>* observers,
                     NodeObserverType* observer) {
  DCHECK(observers);
  DCHECK(observer);
  auto it = std::find(observers->begin(), observers->end(), observer);
  DCHECK(it == observers->end());
  observers->push_back(observer);
}

template <typename NodeObserverType>
void RemoveObserverImpl(std::vector<NodeObserverType*>* observers,
                        NodeObserverType* observer) {
  DCHECK(observers);
  DCHECK(observer);
  // We expect to find the observer in the array.
  auto it = std::find(observers->begin(), observers->end(), observer);
  DCHECK(it != observers->end());
  observers->erase(it);
  // There should only have been one copy of the observer.
  it = std::find(observers->begin(), observers->end(), observer);
  DCHECK(it == observers->end());
}

}  // namespace

GraphImpl::GraphImpl() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

// TODO(chrisha|siggi): Move this to a TearDown, which is invoked by the
// performance manager before the graph destructor.
GraphImpl::~GraphImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Notify graph observers that the graph is being destroyed.
  for (auto* observer : graph_observers_)
    observer->OnBeforeGraphDestroyed(this);

  // Clean up graph owned objects. This causes their TakeFromGraph callbacks to
  // be invoked, and ideally they clean up any observers they may have, etc.
  while (!graph_owned_.empty())
    auto object = TakeFromGraph(graph_owned_.begin()->first);

  // At this point, all typed observers should be empty.
  DCHECK(graph_observers_.empty());
  DCHECK(frame_node_observers_.empty());
  DCHECK(page_node_observers_.empty());
  DCHECK(process_node_observers_.empty());
  DCHECK(system_node_observers_.empty());

  // All observers should have been removed before the graph is deleted.
  // TODO(chrisha): This will disappear, as new observers are allowed to stay
  // attached at graph death.
  DCHECK(observers_.empty());
  // All process nodes should have been removed already.
  DCHECK(processes_by_pid_.empty());

  // Remove the system node from the graph, this should be the only node left.
  ReleaseSystemNode();

  DCHECK(nodes_.empty());
}

void GraphImpl::AddGraphObserver(GraphObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddObserverImpl(&graph_observers_, observer);
}

void GraphImpl::AddFrameNodeObserver(FrameNodeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddObserverImpl(&frame_node_observers_, observer);
}

void GraphImpl::AddPageNodeObserver(PageNodeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddObserverImpl(&page_node_observers_, observer);
}

void GraphImpl::AddProcessNodeObserver(ProcessNodeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddObserverImpl(&process_node_observers_, observer);
}

void GraphImpl::AddSystemNodeObserver(SystemNodeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddObserverImpl(&system_node_observers_, observer);
}

void GraphImpl::RemoveGraphObserver(GraphObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveObserverImpl(&graph_observers_, observer);
}

void GraphImpl::RemoveFrameNodeObserver(FrameNodeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveObserverImpl(&frame_node_observers_, observer);
}

void GraphImpl::RemovePageNodeObserver(PageNodeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveObserverImpl(&page_node_observers_, observer);
}

void GraphImpl::RemoveProcessNodeObserver(ProcessNodeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveObserverImpl(&process_node_observers_, observer);
}

void GraphImpl::RemoveSystemNodeObserver(SystemNodeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveObserverImpl(&system_node_observers_, observer);
}

void GraphImpl::PassToGraph(std::unique_ptr<GraphOwned> graph_owned) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* raw = graph_owned.get();
  DCHECK(!base::Contains(graph_owned_, raw));
  graph_owned_.insert(std::make_pair(raw, std::move(graph_owned)));
  raw->OnPassedToGraph(this);
}

std::unique_ptr<GraphOwned> GraphImpl::TakeFromGraph(GraphOwned* graph_owned) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<GraphOwned> object;
  auto it = graph_owned_.find(graph_owned);
  if (it != graph_owned_.end()) {
    DCHECK_EQ(graph_owned, it->first);
    DCHECK_EQ(graph_owned, it->second.get());
    object = std::move(it->second);
    graph_owned_.erase(it);
    object->OnTakenFromGraph(this);
  }
  return object;
}

const SystemNode* GraphImpl::FindOrCreateSystemNode() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return FindOrCreateSystemNodeImpl();
}

std::vector<const ProcessNode*> GraphImpl::GetAllProcessNodes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetAllNodesOfType<ProcessNodeImpl, const ProcessNode*>();
}

std::vector<const FrameNode*> GraphImpl::GetAllFrameNodes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetAllNodesOfType<FrameNodeImpl, const FrameNode*>();
}

std::vector<const PageNode*> GraphImpl::GetAllPageNodes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetAllNodesOfType<PageNodeImpl, const PageNode*>();
}

void GraphImpl::RegisterObserver(GraphImplObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer->SetGraph(this);
  AddObserverImpl(&observers_, observer);
  observer->OnRegistered();
}

void GraphImpl::UnregisterObserver(GraphImplObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveObserverImpl(&observers_, observer);
  observer->OnUnregistered();
  observer->SetGraph(nullptr);
}

ukm::UkmRecorder* GraphImpl::GetUkmRecorder() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ukm_recorder();
}

uintptr_t GraphImpl::GetImplType() const {
  return kGraphImplType;
}

const void* GraphImpl::GetImpl() const {
  return this;
}

// static
GraphImpl* GraphImpl::FromGraph(const Graph* graph) {
  CHECK_EQ(kGraphImplType, graph->GetImplType());
  return reinterpret_cast<GraphImpl*>(const_cast<void*>(graph->GetImpl()));
}

void GraphImpl::OnNodeAdded(NodeBase* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This handles legacy GraphImplObserver implementation.
  for (auto* observer : observers_) {
    if (observer->ShouldObserve(node)) {
      // TODO(chrisha): Remove this logic once all observers have been migrated.
      switch (node->type()) {
        case NodeTypeEnum::kFrame: {
          FrameNodeImpl::FromNodeBase(node)->AddObserver(observer);
        } break;
        case NodeTypeEnum::kPage: {
          PageNodeImpl::FromNodeBase(node)->AddObserver(observer);
        } break;
        case NodeTypeEnum::kProcess: {
          ProcessNodeImpl::FromNodeBase(node)->AddObserver(observer);
        } break;
        case NodeTypeEnum::kSystem: {
          SystemNodeImpl::FromNodeBase(node)->AddObserver(observer);
        } break;
        case NodeTypeEnum::kInvalidType: {
          NOTREACHED();
        } break;
      }
      observer->OnNodeAdded(node);
    }
  }

  // This handles the strongly typed observer notifications.
  switch (node->type()) {
    case NodeTypeEnum::kFrame: {
      auto* frame_node = FrameNodeImpl::FromNodeBase(node);
      for (auto* observer : frame_node_observers_)
        observer->OnFrameNodeAdded(frame_node);
    } break;
    case NodeTypeEnum::kPage: {
      auto* page_node = PageNodeImpl::FromNodeBase(node);
      for (auto* observer : page_node_observers_)
        observer->OnPageNodeAdded(page_node);
    } break;
    case NodeTypeEnum::kProcess: {
      auto* process_node = ProcessNodeImpl::FromNodeBase(node);
      for (auto* observer : process_node_observers_)
        observer->OnProcessNodeAdded(process_node);
    } break;
    case NodeTypeEnum::kSystem: {
      auto* system_node = SystemNodeImpl::FromNodeBase(node);
      for (auto* observer : system_node_observers_)
        observer->OnSystemNodeAdded(system_node);
    } break;
    case NodeTypeEnum::kInvalidType: {
      NOTREACHED();
    } break;
  }
}

void GraphImpl::OnBeforeNodeRemoved(NodeBase* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This handles the strongly typed observer notifications.
  switch (node->type()) {
    case NodeTypeEnum::kFrame: {
      auto* frame_node = FrameNodeImpl::FromNodeBase(node);
      for (auto* observer : frame_node_observers_)
        observer->OnBeforeFrameNodeRemoved(frame_node);
    } break;
    case NodeTypeEnum::kPage: {
      auto* page_node = PageNodeImpl::FromNodeBase(node);
      for (auto* observer : page_node_observers_)
        observer->OnBeforePageNodeRemoved(page_node);
    } break;
    case NodeTypeEnum::kProcess: {
      auto* process_node = ProcessNodeImpl::FromNodeBase(node);
      for (auto* observer : process_node_observers_)
        observer->OnBeforeProcessNodeRemoved(process_node);
    } break;
    case NodeTypeEnum::kSystem: {
      auto* system_node = SystemNodeImpl::FromNodeBase(node);
      for (auto* observer : system_node_observers_)
        observer->OnBeforeSystemNodeRemoved(system_node);
    } break;
    case NodeTypeEnum::kInvalidType: {
      NOTREACHED();
    } break;
  }

  // Dispatch to the legacy observers.
  for (auto& observer : node->observers())
    observer.OnBeforeNodeRemoved(node);

  // Leave the graph only after the OnBeforeNodeRemoved notification so that the
  // node still observes the graph invariant during that callback.
  node->LeaveGraph();
}

int64_t GraphImpl::GetNextNodeSerializationId() {
  return ++current_node_serialization_id_;
}

SystemNodeImpl* GraphImpl::FindOrCreateSystemNodeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!system_node_) {
    // Create the singleton system node instance. Ownership is taken by the
    // graph.
    system_node_ = std::make_unique<SystemNodeImpl>(this);
    AddNewNode(system_node_.get());
  }

  return system_node_.get();
}

bool GraphImpl::NodeInGraph(const NodeBase* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto& it = nodes_.find(const_cast<NodeBase*>(node));
  return it != nodes_.end();
}

ProcessNodeImpl* GraphImpl::GetProcessNodeByPid(base::ProcessId pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = processes_by_pid_.find(pid);
  if (it == processes_by_pid_.end())
    return nullptr;

  return ProcessNodeImpl::FromNodeBase(it->second);
}

std::vector<ProcessNodeImpl*> GraphImpl::GetAllProcessNodeImpls() const {
  return GetAllNodesOfType<ProcessNodeImpl, ProcessNodeImpl*>();
}

std::vector<FrameNodeImpl*> GraphImpl::GetAllFrameNodeImpls() const {
  return GetAllNodesOfType<FrameNodeImpl, FrameNodeImpl*>();
}

std::vector<PageNodeImpl*> GraphImpl::GetAllPageNodeImpls() const {
  return GetAllNodesOfType<PageNodeImpl, PageNodeImpl*>();
}

size_t GraphImpl::GetNodeAttachedDataCountForTesting(const Node* node,
                                                     const void* key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!node && !key)
    return node_attached_data_map_.size();

  size_t count = 0;
  for (auto& node_data : node_attached_data_map_) {
    if (node && node_data.first.first != node)
      continue;
    if (key && node_data.first.second != key)
      continue;
    ++count;
  }

  return count;
}

void GraphImpl::AddNewNode(NodeBase* new_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = nodes_.insert(new_node);
  DCHECK(it.second);  // Inserted successfully

  // Allow the node to initialize itself now that it's been added.
  new_node->JoinGraph();
  OnNodeAdded(new_node);
}

void GraphImpl::RemoveNode(NodeBase* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnBeforeNodeRemoved(node);

  // Remove any node attached data affiliated with this node.
  const Node* public_node = node->ToNode();
  auto lower =
      node_attached_data_map_.lower_bound(std::make_pair(public_node, nullptr));
  auto upper = node_attached_data_map_.lower_bound(
      std::make_pair(public_node + 1, nullptr));
  node_attached_data_map_.erase(lower, upper);

  // Before removing the node itself.
  size_t erased = nodes_.erase(node);
  DCHECK_EQ(1u, erased);
}

void GraphImpl::BeforeProcessPidChange(ProcessNodeImpl* process,
                                       base::ProcessId new_pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // On Windows, PIDs are aggressively reused, and because not all process
  // creation/death notifications are synchronized, it's possible for more than
  // one process node to have the same PID. To handle this, the second and
  // subsequent registration override earlier registrations, while
  // unregistration will only unregister the current holder of the PID.
  if (process->process_id() != base::kNullProcessId) {
    auto it = processes_by_pid_.find(process->process_id());
    if (it != processes_by_pid_.end() && it->second == process)
      processes_by_pid_.erase(it);
  }
  if (new_pid != base::kNullProcessId)
    processes_by_pid_[new_pid] = process;
}

template <typename NodeType, typename ReturnNodeType>
std::vector<ReturnNodeType> GraphImpl::GetAllNodesOfType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto type = NodeType::Type();
  std::vector<ReturnNodeType> ret;
  for (auto* node : nodes_) {
    if (node->type() == type)
      ret.push_back(NodeType::FromNodeBase(node));
  }
  return ret;
}

void GraphImpl::ReleaseSystemNode() {
  if (!system_node_.get())
    return;
  RemoveNode(system_node_.get());
  system_node_.reset();
}

template <>
const std::vector<FrameNodeObserver*>& GraphImpl::GetObservers() const {
  return frame_node_observers_;
}

template <>
const std::vector<PageNodeObserver*>& GraphImpl::GetObservers() const {
  return page_node_observers_;
}

template <>
const std::vector<ProcessNodeObserver*>& GraphImpl::GetObservers() const {
  return process_node_observers_;
}

template <>
const std::vector<SystemNodeObserver*>& GraphImpl::GetObservers() const {
  return system_node_observers_;
}

}  // namespace performance_manager
