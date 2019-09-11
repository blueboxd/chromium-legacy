// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/performance_manager_impl.h"

#include <atomic>
#include <memory>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
#include "chrome/browser/performance_manager/decorators/freeze_origin_trial_policy_aggregator.h"
#include "chrome/browser/performance_manager/decorators/frozen_frame_aggregator.h"
#include "chrome/browser/performance_manager/decorators/page_almost_idle_decorator.h"
#include "chrome/browser/performance_manager/decorators/process_metrics_decorator.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/policies/policy_features.h"
#include "chrome/browser/performance_manager/graph/policies/working_set_trimmer_policy.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/graph/system_node_impl.h"
#include "chrome/browser/performance_manager/graph/worker_node_impl.h"
#include "chrome/browser/performance_manager/observers/isolation_context_metrics.h"
#include "chrome/browser/performance_manager/observers/metrics_collector.h"
#include "content/public/browser/system_connector.h"
#include "content/public/common/content_features.h"

#if defined(OS_LINUX)
#include "base/allocator/buildflags.h"
#if BUILDFLAG(USE_TCMALLOC)
#include "chrome/browser/performance_manager/graph/policies/dynamic_tcmalloc_policy_linux.h"
#include "chrome/common/performance_manager/mojom/tcmalloc.mojom.h"
#endif  // BUILDFLAG(USE_TCMALLOC)
#endif  // defined(OS_LINUX)

namespace performance_manager {

namespace {

class Singleton {
 public:
  Singleton() = default;
  ~Singleton() = default;

  // Safe to call from any thread.
  PerformanceManagerImpl* Get() {
    return instance_.load(std::memory_order_acquire);
  }

  // Should be called from the main thread only.
  void Set(PerformanceManagerImpl* instance) {
    DCHECK_NE(nullptr, instance);
    auto* old_value = instance_.exchange(instance, std::memory_order_release);
    DCHECK_EQ(nullptr, old_value);
  }

  // Should be called from the main thread only.
  void Clear(PerformanceManagerImpl* instance) {
    DCHECK_NE(nullptr, instance);
    auto* old_value = instance_.exchange(nullptr, std::memory_order_release);
    DCHECK_EQ(instance, old_value);
  }

 private:
  std::atomic<PerformanceManagerImpl*> instance_{nullptr};
};
Singleton g_performance_manager;

scoped_refptr<base::SequencedTaskRunner> CreateTaskRunner() {
  return base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN, base::MayBlock()});
}

}  // namespace

PerformanceManagerImpl::~PerformanceManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(https://crbug.com/966840): Move this to a TearDown function.
  graph_.TearDown();
}

// static
void PerformanceManagerImpl::CallOnGraphImpl(const base::Location& from_here,
                                             GraphImplCallback callback) {
  DCHECK(callback);

  // Passing |pm| unretained is safe as it is actually destroyed on the
  // destination sequence, and g_performance_manager.Get() would return nullptr
  // if its deletion task was already posted.
  auto* pm = g_performance_manager.Get();
  pm->task_runner_->PostTask(
      from_here,
      base::BindOnce(&PerformanceManagerImpl::RunCallbackWithGraphImpl,
                     base::Unretained(pm), std::move(callback)));
}

PerformanceManagerImpl* PerformanceManagerImpl::GetInstance() {
  return g_performance_manager.Get();
}

// static
std::unique_ptr<PerformanceManagerImpl> PerformanceManagerImpl::Create() {
  std::unique_ptr<PerformanceManagerImpl> instance =
      base::WrapUnique(new PerformanceManagerImpl());

  instance->OnStart();
  g_performance_manager.Set(instance.get());

  return instance;
}

// static
void PerformanceManagerImpl::Destroy(
    std::unique_ptr<PerformanceManagerImpl> instance) {
  g_performance_manager.Clear(instance.get());
  instance->task_runner_->DeleteSoon(FROM_HERE, instance.release());
}

std::unique_ptr<FrameNodeImpl> PerformanceManagerImpl::CreateFrameNode(
    ProcessNodeImpl* process_node,
    PageNodeImpl* page_node,
    FrameNodeImpl* parent_frame_node,
    int frame_tree_node_id,
    const base::UnguessableToken& dev_tools_token,
    int32_t browsing_instance_id,
    int32_t site_instance_id) {
  return CreateNodeImpl<FrameNodeImpl>(
      FrameNodeCreationCallback(), process_node, page_node, parent_frame_node,
      frame_tree_node_id, dev_tools_token, browsing_instance_id,
      site_instance_id);
}

std::unique_ptr<FrameNodeImpl> PerformanceManagerImpl::CreateFrameNode(
    ProcessNodeImpl* process_node,
    PageNodeImpl* page_node,
    FrameNodeImpl* parent_frame_node,
    int frame_tree_node_id,
    const base::UnguessableToken& dev_tools_token,
    int32_t browsing_instance_id,
    int32_t site_instance_id,
    FrameNodeCreationCallback creation_callback) {
  return CreateNodeImpl<FrameNodeImpl>(
      std::move(creation_callback), process_node, page_node, parent_frame_node,
      frame_tree_node_id, dev_tools_token, browsing_instance_id,
      site_instance_id);
}

std::unique_ptr<PageNodeImpl> PerformanceManagerImpl::CreatePageNode(
    const WebContentsProxy& contents_proxy,
    const std::string& browser_context_id,
    bool is_visible,
    bool is_audible) {
  return CreateNodeImpl<PageNodeImpl>(base::OnceCallback<void(PageNodeImpl*)>(),
                                      contents_proxy, browser_context_id,
                                      is_visible, is_audible);
}

std::unique_ptr<ProcessNodeImpl> PerformanceManagerImpl::CreateProcessNode(
    RenderProcessHostProxy proxy) {
  return CreateNodeImpl<ProcessNodeImpl>(
      base::OnceCallback<void(ProcessNodeImpl*)>(), proxy);
}

std::unique_ptr<WorkerNodeImpl> PerformanceManagerImpl::CreateWorkerNode(
    const std::string& browser_context_id,
    WorkerNode::WorkerType worker_type,
    ProcessNodeImpl* process_node,
    const GURL& url,
    const base::UnguessableToken& dev_tools_token) {
  return CreateNodeImpl<WorkerNodeImpl>(
      base::OnceCallback<void(WorkerNodeImpl*)>(), browser_context_id,
      worker_type, process_node, url, dev_tools_token);
}

void PerformanceManagerImpl::BatchDeleteNodes(
    std::vector<std::unique_ptr<NodeBase>> nodes) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PerformanceManagerImpl::BatchDeleteNodesImpl,
                                base::Unretained(this), std::move(nodes)));
}

PerformanceManagerImpl::PerformanceManagerImpl()
    : task_runner_(CreateTaskRunner()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

namespace {

// Helper function for adding a node to a graph, and invoking a post-creation
// callback immediately afterwards.
template <typename NodeType>
void AddNodeAndInvokeCreationCallback(
    base::OnceCallback<void(NodeType*)> callback,
    NodeType* node,
    GraphImpl* graph) {
  graph->AddNewNode(node);
  if (callback)
    std::move(callback).Run(node);
}

}  // namespace

template <typename NodeType, typename... Args>
std::unique_ptr<NodeType> PerformanceManagerImpl::CreateNodeImpl(
    base::OnceCallback<void(NodeType*)> creation_callback,
    Args&&... constructor_args) {
  std::unique_ptr<NodeType> new_node = std::make_unique<NodeType>(
      &graph_, std::forward<Args>(constructor_args)...);
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AddNodeAndInvokeCreationCallback<NodeType>,
                                std::move(creation_callback),
                                base::Unretained(new_node.get()),
                                base::Unretained(&graph_)));
  return new_node;
}

void PerformanceManagerImpl::PostDeleteNode(std::unique_ptr<NodeBase> node) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PerformanceManagerImpl::DeleteNodeImpl,
                                base::Unretained(this), std::move(node)));
}

void PerformanceManagerImpl::DeleteNodeImpl(std::unique_ptr<NodeBase> node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  graph_.RemoveNode(node.get());
}

namespace {

void RemoveFrameAndChildrenFromGraph(FrameNodeImpl* frame_node) {
  // Recurse on the first child while there is one.
  while (!frame_node->child_frame_nodes().empty())
    RemoveFrameAndChildrenFromGraph(*(frame_node->child_frame_nodes().begin()));

  // Now that all children are deleted, delete this frame.
  frame_node->graph()->RemoveNode(frame_node);
}

}  // namespace

void PerformanceManagerImpl::BatchDeleteNodesImpl(
    std::vector<std::unique_ptr<NodeBase>> nodes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::flat_set<ProcessNodeImpl*> process_nodes;

  for (auto it = nodes.begin(); it != nodes.end(); ++it) {
    switch ((*it)->type()) {
      case PageNodeImpl::Type(): {
        auto* page_node = PageNodeImpl::FromNodeBase(it->get());

        // Delete the main frame nodes until no more exist.
        while (!page_node->main_frame_nodes().empty())
          RemoveFrameAndChildrenFromGraph(
              *(page_node->main_frame_nodes().begin()));

        graph_.RemoveNode(page_node);
        break;
      }
      case ProcessNodeImpl::Type(): {
        // Keep track of the process nodes for removing once all frames nodes
        // are removed.
        auto* process_node = ProcessNodeImpl::FromNodeBase(it->get());
        process_nodes.insert(process_node);
        break;
      }
      case FrameNodeImpl::Type():
        break;
      case SystemNodeImpl::Type():
      case NodeTypeEnum::kInvalidType:
      default: {
        NOTREACHED();
        break;
      }
    }
  }

  // Remove the process nodes from the graph.
  for (auto* process_node : process_nodes)
    graph_.RemoveNode(process_node);

  // When |nodes| goes out of scope, all nodes are deleted.
}

void PerformanceManagerImpl::OnStart() {
  // Some tests don't initialize the service manager connection, so this class
  // tolerates its absence for tests.
  auto* connector = content::GetSystemConnector();
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PerformanceManagerImpl::OnStartImpl,
                                base::Unretained(this),
                                connector ? connector->Clone() : nullptr));
}

void PerformanceManagerImpl::RunCallbackWithGraphImpl(
    GraphImplCallback graph_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(graph_callback).Run(&graph_);
}

void PerformanceManagerImpl::RunCallbackWithGraph(
    GraphCallback graph_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(graph_callback).Run(&graph_);
}

void PerformanceManagerImpl::OnStartImpl(
    std::unique_ptr<service_manager::Connector> connector) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  graph_.PassToGraph(std::make_unique<FreezeOriginTrialPolicyAggregator>());
  graph_.PassToGraph(std::make_unique<FrozenFrameAggregator>());
  graph_.PassToGraph(std::make_unique<PageAlmostIdleDecorator>());
  graph_.PassToGraph(std::make_unique<IsolationContextMetrics>());
  graph_.PassToGraph(std::make_unique<MetricsCollector>());
  graph_.PassToGraph(std::make_unique<ProcessMetricsDecorator>());

  if (policies::WorkingSetTrimmerPolicy::PlatformSupportsWorkingSetTrim()) {
    graph_.PassToGraph(
        policies::WorkingSetTrimmerPolicy::CreatePolicyForPlatform());
  }

#if defined(OS_LINUX)
#if BUILDFLAG(USE_TCMALLOC)
  if (base::FeatureList::IsEnabled(features::kDynamicTcmallocTuning)) {
    graph_.PassToGraph(std::make_unique<policies::DynamicTcmallocPolicy>());
  }
#endif  // BUILDFLAG(USE_TCMALLOC)
#endif  // defined(OS_LINUX)

  if (connector) {
    ukm_recorder_ = ukm::MojoUkmRecorder::Create(connector.get());
    graph_.set_ukm_recorder(ukm_recorder_.get());
  }
}

}  // namespace performance_manager
