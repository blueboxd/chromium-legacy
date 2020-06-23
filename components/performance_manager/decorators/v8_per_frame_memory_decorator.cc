// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/v8_per_frame_memory_decorator.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/stl_util.h"
#include "base/timer/timer.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace performance_manager {

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

// Private implementations of the node attached data. This keeps the complexity
// out of the header file.

class NodeAttachedFrameData
    : public ExternalNodeAttachedDataImpl<NodeAttachedFrameData> {
 public:
  explicit NodeAttachedFrameData(const FrameNode* frame_node) {}
  ~NodeAttachedFrameData() override = default;

  NodeAttachedFrameData(const NodeAttachedFrameData&) = delete;
  NodeAttachedFrameData& operator=(const NodeAttachedFrameData&) = delete;

  const V8PerFrameMemoryDecorator::FrameData* data() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return data_available_ ? &data_ : nullptr;
  }

 private:
  friend class NodeAttachedProcessData;

  V8PerFrameMemoryDecorator::FrameData data_;
  bool data_available_ = false;
  SEQUENCE_CHECKER(sequence_checker_);
};

class NodeAttachedProcessData
    : public ExternalNodeAttachedDataImpl<NodeAttachedProcessData> {
 public:
  explicit NodeAttachedProcessData(const ProcessNode* process_node)
      : process_node_(process_node) {}
  ~NodeAttachedProcessData() override = default;

  NodeAttachedProcessData(const NodeAttachedProcessData&) = delete;
  NodeAttachedProcessData& operator=(const NodeAttachedProcessData&) = delete;

  const V8PerFrameMemoryDecorator::ProcessData* data() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return data_available_ ? &data_ : nullptr;
  }

  void Initialize(const V8PerFrameMemoryDecorator* decorator);
  void ScheduleNextMeasurement();

 private:
  void StartMeasurement();
  void EnsureRemote();
  void OnPerFrameV8MemoryUsageData(
      performance_manager::mojom::PerProcessV8MemoryUsageDataPtr result);

  const ProcessNode* const process_node_;
  const V8PerFrameMemoryDecorator* decorator_ = nullptr;

  mojo::Remote<performance_manager::mojom::V8PerFrameMemoryReporter>
      resource_usage_reporter_;

  enum class State {
    kUninitialized,
    kWaiting,    // Waiting to take a measurement.
    kMeasuring,  // Waiting for measurement results.
    kIdle,       // No measurements scheduled.
  };
  State state_ = State::kUninitialized;

  // Used to schedule the next measurement.
  base::TimeTicks last_request_time_;
  base::OneShotTimer timer_;

  V8PerFrameMemoryDecorator::ProcessData data_;
  bool data_available_ = false;
  SEQUENCE_CHECKER(sequence_checker_);
};

void NodeAttachedProcessData::Initialize(
    const V8PerFrameMemoryDecorator* decorator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(nullptr, decorator_);
  decorator_ = decorator;
  DCHECK_EQ(state_, State::kUninitialized);

  state_ = State::kWaiting;
  ScheduleNextMeasurement();
}

void NodeAttachedProcessData::ScheduleNextMeasurement() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(nullptr, decorator_);
  DCHECK_NE(state_, State::kUninitialized);

  if (state_ == State::kMeasuring) {
    // Don't restart the timer until the current measurement finishes.
    // ScheduleNextMeasurement will be called again at that point.
    return;
  }

  if (decorator_->GetMinTimeBetweenRequestsPerProcess().is_zero()) {
    // All measurements have been cancelled.
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
      last_request_time_ + decorator_->GetMinTimeBetweenRequestsPerProcess();
  timer_.Start(FROM_HERE, next_request_time - base::TimeTicks::Now(), this,
               &NodeAttachedProcessData::StartMeasurement);
}

void NodeAttachedProcessData::StartMeasurement() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kWaiting);
  state_ = State::kMeasuring;
  last_request_time_ = base::TimeTicks::Now();

  EnsureRemote();
  resource_usage_reporter_->GetPerFrameV8MemoryUsageData(
      base::BindOnce(&NodeAttachedProcessData::OnPerFrameV8MemoryUsageData,
                     base::Unretained(this)));
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

V8PerFrameMemoryDecorator::MeasurementRequest::MeasurementRequest(
    const base::TimeDelta& sample_frequency)
    : sample_frequency_(sample_frequency) {
  DCHECK_GT(sample_frequency_, base::TimeDelta());
}

V8PerFrameMemoryDecorator::MeasurementRequest::MeasurementRequest(
    const base::TimeDelta& sample_frequency,
    Graph* graph)
    : sample_frequency_(sample_frequency) {
  DCHECK_GT(sample_frequency_, base::TimeDelta());
  StartMeasurement(graph);
}

V8PerFrameMemoryDecorator::MeasurementRequest::~MeasurementRequest() {
  if (decorator_)
    decorator_->RemoveMeasurementRequest(this);
  // TODO(crbug.com/1080672): Delete the decorator and its NodeAttachedData
  // when the last request is destroyed. Make sure this doesn't mess up any
  // measurement that's already in progress.
}

void V8PerFrameMemoryDecorator::MeasurementRequest::StartMeasurement(
    Graph* graph) {
  DCHECK_EQ(nullptr, decorator_);
  decorator_ = graph->GetRegisteredObjectAs<V8PerFrameMemoryDecorator>();
  if (!decorator_) {
    // Create the decorator when the first measurement starts.
    auto decorator_ptr = std::make_unique<V8PerFrameMemoryDecorator>();
    decorator_ = decorator_ptr.get();
    graph->PassToGraph(std::move(decorator_ptr));
  }

  decorator_->AddMeasurementRequest(this);
}

void V8PerFrameMemoryDecorator::MeasurementRequest::OnDecoratorUnregistered() {
  decorator_ = nullptr;
}

const V8PerFrameMemoryDecorator::FrameData*
V8PerFrameMemoryDecorator::FrameData::ForFrameNode(const FrameNode* node) {
  auto* node_data = NodeAttachedFrameData::Get(node);
  return node_data ? node_data->data() : nullptr;
}

const V8PerFrameMemoryDecorator::ProcessData*
V8PerFrameMemoryDecorator::ProcessData::ForProcessNode(
    const ProcessNode* node) {
  auto* node_data = NodeAttachedProcessData::Get(node);
  return node_data ? node_data->data() : nullptr;
}

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
  for (MeasurementRequest* request : measurement_requests_) {
    request->OnDecoratorUnregistered();
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

  NodeAttachedProcessData* process_data =
      NodeAttachedProcessData::GetOrCreate(process_node);
  process_data->Initialize(this);
}

base::Value V8PerFrameMemoryDecorator::DescribeFrameNodeData(
    const FrameNode* frame_node) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const FrameData* const frame_data = FrameData::ForFrameNode(frame_node);
  if (!frame_data)
    return base::Value();

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("v8_bytes_used", frame_data->v8_bytes_used());
  return dict;
}

base::Value V8PerFrameMemoryDecorator::DescribeProcessNodeData(
    const ProcessNode* process_node) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const ProcessData* const process_data =
      ProcessData::ForProcessNode(process_node);
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
    MeasurementRequest* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(request);
  DCHECK(!base::Contains(measurement_requests_, request))
      << "MeasurementRequest object added twice";
  // Each user of this decorator is expected to issue a single
  // MeasurementRequest, so the size of measurement_requests_ is too low to
  // make the complexity of real priority queue worthwhile.
  for (std::vector<MeasurementRequest*>::const_iterator it =
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
    MeasurementRequest* request) {
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

V8PerFrameMemoryRequest::V8PerFrameMemoryRequest(
    const base::TimeDelta& sample_frequency)
    : request_(std::make_unique<V8PerFrameMemoryDecorator::MeasurementRequest>(
          sample_frequency)) {
  // Unretained is safe since the destructor will run on the same sequence as
  // StartMeasurement.
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          &V8PerFrameMemoryDecorator::MeasurementRequest::StartMeasurement,
          base::Unretained(request_.get())));
}

V8PerFrameMemoryRequest::~V8PerFrameMemoryRequest() {
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<V8PerFrameMemoryDecorator::MeasurementRequest>
                 request) { request.reset(); },
          std::move(request_)));
}

}  // namespace performance_manager
