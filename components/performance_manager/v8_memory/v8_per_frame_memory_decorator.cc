// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/v8_memory/v8_per_frame_memory_decorator.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/timer/timer.h"
#include "base/util/type_safety/pass_key.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/render_frame_host_proxy.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace performance_manager {

namespace v8_memory {

// This class is allowed to access
// V8PerFrameMemoryDecorator::NotifyObserversOnMeasurementAvailable.
class V8PerFrameMemoryDecorator::ObserverNotifier {
 public:
  void NotifyObserversOnMeasurementAvailable(const ProcessNode* process_node) {
    auto* decorator =
        V8PerFrameMemoryDecorator::GetFromGraph(process_node->GetGraph());
    if (decorator)
      decorator->NotifyObserversOnMeasurementAvailable(
          util::PassKey<ObserverNotifier>(), process_node);
  }
};

namespace {

// Forwards the pending receiver to the RenderProcessHost and binds it on the
// UI thread.
void BindReceiverOnUIThread(
    mojo::PendingReceiver<performance_manager::mojom::V8PerFrameMemoryReporter>
        pending_receiver,
    RenderProcessHostProxy proxy) {
  auto* render_process_host = proxy.Get();
  if (render_process_host) {
    render_process_host->BindReceiver(std::move(pending_receiver));
  }
}

internal::BindV8PerFrameMemoryReporterCallback* g_test_bind_callback = nullptr;

// Per-frame memory measurement involves the following classes that live on the
// PM sequence:
//
// V8PerFrameMemoryDecorator: Central rendezvous point. Coordinates
//     V8PerFrameMemoryRequest and V8PerFrameMemoryObserver objects. Owned by
//     the graph; created the first time
//     V8PerFrameMemoryRequest::StartMeasurement is called.
//     TODO(b/1080672): Currently this lives forever; should be cleaned up when
//     there are no more measurements scheduled.
//
// V8PerFrameMemoryRequest: Indicates that a caller wants memory to be measured
//     at a specific interval. Owned by the caller but must live on the PM
//     sequence. V8PerFrameMemoryRequest objects register themselves with
//     V8PerFrameMemoryDecorator on creation and unregister themselves on
//     deletion, which cancels the corresponding measurement.
//
// NodeAttachedProcessData: Private class that schedules measurements and holds
//     the results for an individual process. Owned by the ProcessNode; created
//     when measurements start.
//     TODO(b/1080672): Currently this lives forever; should be cleaned up when
//     there are no more measurements scheduled.
//
// V8PerFrameMemoryProcessData: Public accessor to the measurement results held
//     in a NodeAttachedProcessData, which owns it.
//
// NodeAttachedFrameData: Private class that holds the measurement results for
//     a frame. Owned by the FrameNode; created when a measurement result
//     arrives.
//     TODO(b/1080672): Currently this lives forever; should be cleaned up when
//     there are no more measurements scheduled.
//
// V8PerFrameMemoryFrameData: Public accessor to the measurement results held
//     in a NodeAttachedFrameData, which owns it.
//
// V8PerFrameMemoryObserver: Callers can implement this and register with
//     V8PerFrameMemoryDecorator::AddObserver() to be notified when
//     measurements are available for a process. Owned by the caller but must
//     live on the PM sequence.
//
// Additional wrapper classes can access these classes from other sequences:
//
// V8PerFrameMemoryRequestAnySeq: Wraps V8PerFrameMemoryRequest. Owned by the
//     caller and lives on any sequence.
//
// V8PerFrameMemoryObserverAnySeq: Callers can implement this and register it
//     with V8PerFrameMemoryRequestAnySeq::AddObserver() to be notified when
//     measurements are available for a process. Owned by the caller and lives
//     on the same sequence as the V8PerFrameMemoryRequestAnySeq.

////////////////////////////////////////////////////////////////////////////////
// NodeAttachedFrameData

class NodeAttachedFrameData
    : public ExternalNodeAttachedDataImpl<NodeAttachedFrameData> {
 public:
  explicit NodeAttachedFrameData(const FrameNode* frame_node) {}
  ~NodeAttachedFrameData() override = default;

  NodeAttachedFrameData(const NodeAttachedFrameData&) = delete;
  NodeAttachedFrameData& operator=(const NodeAttachedFrameData&) = delete;

  const V8PerFrameMemoryFrameData* data() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return data_available_ ? &data_ : nullptr;
  }

 private:
  friend class NodeAttachedProcessData;

  V8PerFrameMemoryFrameData data_;
  bool data_available_ = false;
  SEQUENCE_CHECKER(sequence_checker_);
};

////////////////////////////////////////////////////////////////////////////////
// NodeAttachedProcessData

class NodeAttachedProcessData
    : public ExternalNodeAttachedDataImpl<NodeAttachedProcessData> {
 public:
  explicit NodeAttachedProcessData(const ProcessNode* process_node);
  ~NodeAttachedProcessData() override = default;

  NodeAttachedProcessData(const NodeAttachedProcessData&) = delete;
  NodeAttachedProcessData& operator=(const NodeAttachedProcessData&) = delete;

  const V8PerFrameMemoryProcessData* data() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return data_available_ ? &data_ : nullptr;
  }

  void ScheduleNextMeasurement();

 private:
  void StartMeasurement();
  void EnsureRemote();
  void OnPerFrameV8MemoryUsageData(
      performance_manager::mojom::PerProcessV8MemoryUsageDataPtr result);

  const ProcessNode* const process_node_;

  mojo::Remote<performance_manager::mojom::V8PerFrameMemoryReporter>
      resource_usage_reporter_;

  enum class State {
    kWaiting,    // Waiting to take a measurement.
    kMeasuring,  // Waiting for measurement results.
    kIdle,       // No measurements scheduled.
  };
  State state_ = State::kIdle;

  // Used to schedule the next measurement.
  base::TimeTicks last_request_time_;
  base::OneShotTimer timer_;

  V8PerFrameMemoryProcessData data_;
  bool data_available_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<NodeAttachedProcessData> weak_factory_{this};
};

NodeAttachedProcessData::NodeAttachedProcessData(
    const ProcessNode* process_node)
    : process_node_(process_node) {
  ScheduleNextMeasurement();
}

void NodeAttachedProcessData::ScheduleNextMeasurement() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ == State::kMeasuring) {
    // Don't restart the timer until the current measurement finishes.
    // ScheduleNextMeasurement will be called again at that point.
    return;
  }

  auto* decorator =
      V8PerFrameMemoryDecorator::GetFromGraph(process_node_->GetGraph());
  if (!decorator ||
      decorator->GetMinTimeBetweenRequestsPerProcess().is_zero()) {
    // All measurements have been cancelled, or decorator was removed from
    // graph.
    state_ = State::kIdle;
    timer_.Stop();
    last_request_time_ = base::TimeTicks();
    return;
  }

  state_ = State::kWaiting;
  if (last_request_time_.is_null()) {
    // This is the first measurement. Perform it immediately.
    StartMeasurement();
    return;
  }

  base::TimeTicks next_request_time =
      last_request_time_ + decorator->GetMinTimeBetweenRequestsPerProcess();
  timer_.Start(FROM_HERE, next_request_time - base::TimeTicks::Now(), this,
               &NodeAttachedProcessData::StartMeasurement);
}

void NodeAttachedProcessData::StartMeasurement() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kWaiting);
  state_ = State::kMeasuring;
  last_request_time_ = base::TimeTicks::Now();

  EnsureRemote();

  // TODO(b/1080672): WeakPtr is used in case NodeAttachedProcessData is
  // cleaned up while a request to a renderer is outstanding. Currently this
  // never actually happens (it is destroyed only when the graph is torn down,
  // which should happen after renderers are destroyed). Should clean up
  // NodeAttachedProcessData when the last V8PerFrameMemoryRequest is deleted,
  // which could happen at any time.
  resource_usage_reporter_->GetPerFrameV8MemoryUsageData(
      base::BindOnce(&NodeAttachedProcessData::OnPerFrameV8MemoryUsageData,
                     weak_factory_.GetWeakPtr()));
}

void NodeAttachedProcessData::OnPerFrameV8MemoryUsageData(
    performance_manager::mojom::PerProcessV8MemoryUsageDataPtr result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kMeasuring);

  // Distribute the data to the frames.
  // If a frame doesn't have corresponding data in the result, clear any data
  // it may have had. Any datum in the result that doesn't correspond to an
  // existing frame is likewise accured to unassociated usage.
  uint64_t unassociated_v8_bytes_used = result->unassociated_bytes_used;

  // Create a mapping from token to per-frame usage for the merge below.
  std::vector<std::pair<FrameToken, mojom::PerFrameV8MemoryUsageDataPtr>> tmp;
  for (auto& entry : result->associated_memory) {
    tmp.emplace_back(
        std::make_pair(FrameToken(entry->frame_token), std::move(entry)));
  }
  DCHECK_EQ(tmp.size(), result->associated_memory.size());

  base::flat_map<FrameToken, mojom::PerFrameV8MemoryUsageDataPtr>
      associated_memory(std::move(tmp));
  // Validate that the frame tokens were all unique. If there are duplicates,
  // the map will arbirarily drop all but one record per unique token.
  DCHECK_EQ(associated_memory.size(), result->associated_memory.size());

  base::flat_set<const FrameNode*> frame_nodes = process_node_->GetFrameNodes();
  for (const FrameNode* frame_node : frame_nodes) {
    auto it = associated_memory.find(frame_node->GetFrameToken());
    if (it == associated_memory.end()) {
      // No data for this node, clear any data associated with it.
      NodeAttachedFrameData::Destroy(frame_node);
    } else {
      // There should always be data for the main isolated world for each frame.
      DCHECK(base::Contains(it->second->associated_bytes, 0));

      NodeAttachedFrameData* frame_data =
          NodeAttachedFrameData::GetOrCreate(frame_node);
      for (const auto& kv : it->second->associated_bytes) {
        if (kv.first == 0) {
          frame_data->data_available_ = true;
          frame_data->data_.set_v8_bytes_used(kv.second->bytes_used);
        } else {
          // TODO(crbug.com/1080672): Where to stash the rest of the data?
        }
      }

      // Clear this datum as its usage has been consumed.
      associated_memory.erase(it);
    }
  }

  for (const auto& it : associated_memory) {
    // Accrue the data for non-existent frames to unassociated bytes.
    unassociated_v8_bytes_used += it.second->associated_bytes[0]->bytes_used;
  }

  data_available_ = true;
  data_.set_unassociated_v8_bytes_used(unassociated_v8_bytes_used);

  // Schedule another measurement for this process node.
  state_ = State::kIdle;
  ScheduleNextMeasurement();

  V8PerFrameMemoryDecorator::ObserverNotifier()
      .NotifyObserversOnMeasurementAvailable(process_node_);
}

void NodeAttachedProcessData::EnsureRemote() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (resource_usage_reporter_.is_bound())
    return;

  // This interface is implemented in //content/renderer/performance_manager.
  mojo::PendingReceiver<performance_manager::mojom::V8PerFrameMemoryReporter>
      pending_receiver = resource_usage_reporter_.BindNewPipeAndPassReceiver();

  RenderProcessHostProxy proxy = process_node_->GetRenderProcessHostProxy();

  if (g_test_bind_callback) {
    g_test_bind_callback->Run(std::move(pending_receiver), std::move(proxy));
  } else {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&BindReceiverOnUIThread, std::move(pending_receiver),
                       std::move(proxy)));
  }
}

}  // namespace

namespace internal {

void SetBindV8PerFrameMemoryReporterCallbackForTesting(
    BindV8PerFrameMemoryReporterCallback* callback) {
  g_test_bind_callback = callback;
}

}  // namespace internal

////////////////////////////////////////////////////////////////////////////////
// V8PerFrameMemoryRequest

V8PerFrameMemoryRequest::V8PerFrameMemoryRequest(
    const base::TimeDelta& sample_frequency)
    : sample_frequency_(sample_frequency) {
  DCHECK_GT(sample_frequency_, base::TimeDelta());
}

V8PerFrameMemoryRequest::V8PerFrameMemoryRequest(
    const base::TimeDelta& sample_frequency,
    Graph* graph)
    : sample_frequency_(sample_frequency) {
  DCHECK_GT(sample_frequency_, base::TimeDelta());
  StartMeasurement(graph);
}

// This constructor is called from the V8PerFrameMemoryRequestAnySeq's
// sequence.
V8PerFrameMemoryRequest::V8PerFrameMemoryRequest(
    util::PassKey<V8PerFrameMemoryRequestAnySeq>,
    const base::TimeDelta& sample_frequency,
    base::WeakPtr<V8PerFrameMemoryRequestAnySeq> off_sequence_request)
    : sample_frequency_(sample_frequency),
      off_sequence_request_(std::move(off_sequence_request)),
      off_sequence_request_sequence_(base::SequencedTaskRunnerHandle::Get()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  // Unretained is safe since |this| will be destroyed on the graph sequence.
  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindOnce(&V8PerFrameMemoryRequest::StartMeasurement,
                                base::Unretained(this)));
}

V8PerFrameMemoryRequest::~V8PerFrameMemoryRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (decorator_)
    decorator_->RemoveMeasurementRequest(
        util::PassKey<V8PerFrameMemoryRequest>(), this);
  // TODO(crbug.com/1080672): Delete the decorator and its NodeAttachedData
  // when the last request is destroyed. Make sure this doesn't mess up any
  // measurement that's already in progress.
}

void V8PerFrameMemoryRequest::StartMeasurement(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(nullptr, decorator_);
  decorator_ = V8PerFrameMemoryDecorator::GetFromGraph(graph);
  if (!decorator_) {
    // Create the decorator when the first measurement starts.
    auto decorator_ptr = std::make_unique<V8PerFrameMemoryDecorator>();
    decorator_ = decorator_ptr.get();
    graph->PassToGraph(std::move(decorator_ptr));
  }

  decorator_->AddMeasurementRequest(util::PassKey<V8PerFrameMemoryRequest>(),
                                    this);
}

void V8PerFrameMemoryRequest::AddObserver(V8PerFrameMemoryObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void V8PerFrameMemoryRequest::RemoveObserver(
    V8PerFrameMemoryObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
}

void V8PerFrameMemoryRequest::OnDecoratorUnregistered(
    util::PassKey<V8PerFrameMemoryDecorator>) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  decorator_ = nullptr;
}

void V8PerFrameMemoryRequest::NotifyObserversOnMeasurementAvailable(
    util::PassKey<V8PerFrameMemoryDecorator>,
    const ProcessNode* process_node) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto* process_data =
      V8PerFrameMemoryProcessData::ForProcessNode(process_node);
  DCHECK(process_data);
  for (V8PerFrameMemoryObserver& observer : observers_)
    observer.OnV8MemoryMeasurementAvailable(process_node, process_data);

  // If this request was made from off-sequence, notify its off-sequence
  // observers with a copy of the process and frame data.
  if (off_sequence_request_.MaybeValid()) {
    using FrameAndData =
        std::pair<content::GlobalFrameRoutingId, V8PerFrameMemoryFrameData>;
    std::vector<FrameAndData> all_frame_data;
    process_node->VisitFrameNodes(base::BindRepeating(
        [](std::vector<FrameAndData>* all_frame_data,
           const FrameNode* frame_node) {
          const auto* frame_data =
              V8PerFrameMemoryFrameData::ForFrameNode(frame_node);
          if (frame_data) {
            all_frame_data->push_back(std::make_pair(
                frame_node->GetRenderFrameHostProxy().global_frame_routing_id(),
                *frame_data));
          }
          return true;
        },
        base::Unretained(&all_frame_data)));
    off_sequence_request_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(&V8PerFrameMemoryRequestAnySeq::
                           NotifyObserversOnMeasurementAvailable,
                       off_sequence_request_,
                       util::PassKey<V8PerFrameMemoryRequest>(),
                       process_node->GetRenderProcessHostId(), *process_data,
                       V8PerFrameMemoryObserverAnySeq::FrameDataMap(
                           std::move(all_frame_data))));
  }
}

////////////////////////////////////////////////////////////////////////////////
// V8PerFrameMemoryFrameData

const V8PerFrameMemoryFrameData* V8PerFrameMemoryFrameData::ForFrameNode(
    const FrameNode* node) {
  auto* node_data = NodeAttachedFrameData::Get(node);
  return node_data ? node_data->data() : nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// V8PerFrameMemoryProcessData

const V8PerFrameMemoryProcessData* V8PerFrameMemoryProcessData::ForProcessNode(
    const ProcessNode* node) {
  auto* node_data = NodeAttachedProcessData::Get(node);
  return node_data ? node_data->data() : nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// V8PerFrameMemoryDecorator

V8PerFrameMemoryDecorator::V8PerFrameMemoryDecorator() = default;

V8PerFrameMemoryDecorator::~V8PerFrameMemoryDecorator() {
  DCHECK(measurement_requests_.empty());
}

void V8PerFrameMemoryDecorator::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(nullptr, graph_);
  graph_ = graph;

  graph->RegisterObject(this);

  // Iterate over the existing process nodes to put them under observation.
  for (const ProcessNode* process_node : graph->GetAllProcessNodes())
    OnProcessNodeAdded(process_node);

  graph->AddProcessNodeObserver(this);
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(
      this, "V8PerFrameMemoryDecorator");
}

void V8PerFrameMemoryDecorator::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(graph, graph_);
  for (V8PerFrameMemoryRequest* request : measurement_requests_) {
    request->OnDecoratorUnregistered(
        util::PassKey<V8PerFrameMemoryDecorator>());
  }
  measurement_requests_.clear();
  UpdateProcessMeasurementSchedules();

  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
  graph->RemoveProcessNodeObserver(this);
  graph->UnregisterObject(this);
  graph_ = nullptr;
}

void V8PerFrameMemoryDecorator::OnProcessNodeAdded(
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(nullptr, NodeAttachedProcessData::Get(process_node));

  // Only renderer processes have frames. Don't attempt to connect to other
  // process types.
  if (process_node->GetProcessType() != content::PROCESS_TYPE_RENDERER)
    return;

  // Creating the NodeAttachedProcessData will start a measurement.
  NodeAttachedProcessData::GetOrCreate(process_node);
}

base::Value V8PerFrameMemoryDecorator::DescribeFrameNodeData(
    const FrameNode* frame_node) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto* const frame_data =
      V8PerFrameMemoryFrameData::ForFrameNode(frame_node);
  if (!frame_data)
    return base::Value();

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("v8_bytes_used", frame_data->v8_bytes_used());
  return dict;
}

base::Value V8PerFrameMemoryDecorator::DescribeProcessNodeData(
    const ProcessNode* process_node) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto* const process_data =
      V8PerFrameMemoryProcessData::ForProcessNode(process_node);
  if (!process_data)
    return base::Value();

  DCHECK_EQ(content::PROCESS_TYPE_RENDERER, process_node->GetProcessType());

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("unassociated_v8_bytes_used",
                 process_data->unassociated_v8_bytes_used());
  return dict;
}

base::TimeDelta V8PerFrameMemoryDecorator::GetMinTimeBetweenRequestsPerProcess()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return measurement_requests_.empty()
             ? base::TimeDelta()
             : measurement_requests_.front()->sample_frequency();
}

void V8PerFrameMemoryDecorator::AddMeasurementRequest(
    util::PassKey<V8PerFrameMemoryRequest> key,
    V8PerFrameMemoryRequest* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(request);
  DCHECK(!base::Contains(measurement_requests_, request))
      << "V8PerFrameMemoryRequest object added twice";
  // Each user of this decorator is expected to issue a single
  // V8PerFrameMemoryRequest, so the size of measurement_requests_ is too low
  // to make the complexity of real priority queue worthwhile.
  for (std::vector<V8PerFrameMemoryRequest*>::const_iterator it =
           measurement_requests_.begin();
       it != measurement_requests_.end(); ++it) {
    if (request->sample_frequency() < (*it)->sample_frequency()) {
      measurement_requests_.insert(it, request);
      UpdateProcessMeasurementSchedules();
      return;
    }
  }
  measurement_requests_.push_back(request);
  UpdateProcessMeasurementSchedules();
}

void V8PerFrameMemoryDecorator::RemoveMeasurementRequest(
    util::PassKey<V8PerFrameMemoryRequest> key,
    V8PerFrameMemoryRequest* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(request);
  size_t num_erased = base::Erase(measurement_requests_, request);
  DCHECK_EQ(num_erased, 1ULL);
  UpdateProcessMeasurementSchedules();
}

void V8PerFrameMemoryDecorator::UpdateProcessMeasurementSchedules() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(graph_);
#if DCHECK_IS_ON()
  // Check the data invariant on measurement_requests_, which will be used by
  // ScheduleNextMeasurement.
  for (size_t i = 1; i < measurement_requests_.size(); ++i) {
    DCHECK(measurement_requests_[i - 1]);
    DCHECK(measurement_requests_[i]);
    DCHECK_LE(measurement_requests_[i - 1]->sample_frequency(),
              measurement_requests_[i]->sample_frequency());
  }
#endif
  for (const ProcessNode* node : graph_->GetAllProcessNodes()) {
    NodeAttachedProcessData* process_data = NodeAttachedProcessData::Get(node);
    if (!process_data) {
      DCHECK_NE(content::PROCESS_TYPE_RENDERER, node->GetProcessType())
          << "NodeAttachedProcessData should have been created for all "
             "renderer processes in OnProcessNodeAdded.";
      continue;
    }
    process_data->ScheduleNextMeasurement();
  }
}

void V8PerFrameMemoryDecorator::NotifyObserversOnMeasurementAvailable(
    util::PassKey<ObserverNotifier> key,
    const ProcessNode* process_node) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (V8PerFrameMemoryRequest* request : measurement_requests_) {
    request->NotifyObserversOnMeasurementAvailable(
        util::PassKey<V8PerFrameMemoryDecorator>(), process_node);
  }
}

////////////////////////////////////////////////////////////////////////////////
// V8PerFrameMemoryRequestAnySeq

V8PerFrameMemoryRequestAnySeq::V8PerFrameMemoryRequestAnySeq(
    const base::TimeDelta& sample_frequency) {
  // |request_| must be initialized in the constructor body so that
  // |weak_factory_| is completely constructed.
  //
  // Can't use make_unique since this calls the private any-sequence
  // constructor. After construction the V8PerFrameMemoryRequest must only be
  // accessed on the graph sequence.
  request_ = base::WrapUnique(new V8PerFrameMemoryRequest(
      util::PassKey<V8PerFrameMemoryRequestAnySeq>(), sample_frequency,
      weak_factory_.GetWeakPtr()));
}

V8PerFrameMemoryRequestAnySeq::~V8PerFrameMemoryRequestAnySeq() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindOnce(
                     [](std::unique_ptr<V8PerFrameMemoryRequest> request) {
                       request.reset();
                     },
                     std::move(request_)));
}

void V8PerFrameMemoryRequestAnySeq::AddObserver(
    V8PerFrameMemoryObserverAnySeq* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void V8PerFrameMemoryRequestAnySeq::RemoveObserver(
    V8PerFrameMemoryObserverAnySeq* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
}

void V8PerFrameMemoryRequestAnySeq::NotifyObserversOnMeasurementAvailable(
    util::PassKey<V8PerFrameMemoryRequest>,
    RenderProcessHostId render_process_host_id,
    const V8PerFrameMemoryProcessData& process_data,
    const V8PerFrameMemoryObserverAnySeq::FrameDataMap& frame_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (V8PerFrameMemoryObserverAnySeq& observer : observers_)
    observer.OnV8MemoryMeasurementAvailable(render_process_host_id,
                                            process_data, frame_data);
}

}  // namespace v8_memory

}  // namespace performance_manager
