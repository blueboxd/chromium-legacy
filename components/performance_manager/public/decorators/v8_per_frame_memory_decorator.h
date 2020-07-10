// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_V8_PER_FRAME_MEMORY_DECORATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_V8_PER_FRAME_MEMORY_DECORATOR_H_

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/common/performance_manager/v8_per_frame_memory.mojom.h"

namespace performance_manager {

// A decorator that queries each renderer process for the amount of memory used
// by V8 in each frame.
//
// To start sampling create a MeasurementRequest object that specifies how
// often to request a memory measurement. Delete the object when you no longer
// need measurements. Measurement involves some overhead so choose the lowest
// sampling frequency your use case needs. The decorator will use the highest
// sampling frequency that any caller requests, and stop measurements entirely
// when no more MeasurementRequest objects exist.
//
// When measurements are available the decorator attaches them to FrameData and
// ProcessData objects that can be retrieved with FrameData::ForFrameNode and
// ProcessData::ForProcessNode. ProcessData objects can be cleaned up when
// MeasurementRequest objects are deleted so callers must save the measurements
// they are interested in before releasing their MeasurementRequest.
//
// Callers can be notified when a request is available by implementing
// V8PerFrameMemoryDecorator::Observer.
//
// MeasurementRequest, FrameData and ProcessData must all be accessed on the
// graph sequence, and Observer::OnV8MemoryMeasurementAvailable will be called
// on this sequence. To request memory measurements from another sequence use
// the V8PerFrameMemoryRequestAnySeq and V8PerFrameMemoryObserverAnySeq
// wrappers.
//
// Usage:
//
// Take a memory measurement every 30 seconds and poll for updates:
//
//   class MemoryPoller {
//    public:
//     MemoryPoller() {
//       PerformanceManager::CallOnGraph(FROM_HERE,
//           base::BindOnce(&Start, base::Unretained(this)));
//     }
//
//     void Start(Graph* graph) {
//       DCHECK_ON_GRAPH_SEQUENCE(graph);
//       request_ =
//           std::make_unique<V8PerFrameMemoryDecorator::MeasurementRequest>(
//               base::TimeDelta::FromSeconds(30));
//       request_->StartMeasurement(graph);
//
//       // Periodically check Process and Frame nodes for the latest results.
//       timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(30),
//           base::BindRepeating(&GetResults, base::Unretained(this),
//                               base::Unretained(graph)));
//     }
//
//     void GetResults(Graph* graph) {
//       DCHECK_ON_GRAPH_SEQUENCE(graph);
//       for (auto* process_node : graph->GetAllProcessNodes()) {
//         auto* process_data =
//             V8PerFrameMemoryDecorator::ProcessData::ForProcess(process_node);
//         if (process_data) {
//           LOG(INFO) << "Process " << process_node->GetProcessId() <<
//               " reported " << process_data->unassociated_v8_bytes_used() <<
//               " bytes of V8 memory that wasn't associated with a frame.";
//         }
//         for (auto* frame_node : process_node->GetFrameNodes()) {
//           auto* frame_data =
//               V8PerFrameMemoryDecorator::FrameData::ForFrame(frame_node);
//           if (frame_data) {
//             LOG(INFO) << "Frame " << frame_node->GetFrameToken().value() <<
//                 " reported " << frame_data->v8_bytes_used() <<
//                 " bytes of V8 memory in its main world.";
//           }
//         }
//     }
//
//     void Stop(Graph* graph) {
//       DCHECK_ON_GRAPH_SEQUENCE(graph);
//       // Measurements stop when |request_| is deleted.
//       request_.reset();
//       timer_.Stop();
//     }
//
//    private:
//     std::unique_ptr<V8PerFrameMemoryDecorator::MeasurementRequest> request_;
//     base::RepeatingTimer timer_;
//   };
//
// Take a memory measurement every 2 minutes and register an observer for the
// results:
//
//   class Observer : public V8PerFrameMemoryDecorator::Observer {
//    public:
//     // Called on the PM sequence for each process.
//     void OnV8MemoryMeasurementAvailable(
//         const ProcessNode* const process_node) override {
//       auto* process_data =
//           V8PerFrameMemoryDecorator::ProcessData::ForProcess(process_node);
//       if (process_data) {
//         LOG(INFO) << "Process " << process_node->GetProcessId() <<
//             " reported " << process_data->unassociated_v8_bytes_used() <<
//             " bytes of V8 memory that wasn't associated with a frame.";
//       }
//       for (auto* frame_node : process_node->GetFrameNodes()) {
//         auto* frame_data =
//             V8PerFrameMemoryDecorator::FrameData::ForFrame(frame_node);
//         if (frame_data) {
//           LOG(INFO) << "Frame " << frame_node->GetFrameToken().value() <<
//               " reported " << frame_data->v8_bytes_used() <<
//               " bytes of V8 memory in its main world.";
//         }
//       }
//      }
//   };
//
//   class MemoryMonitor {
//    public:
//     MemoryMonitor() {
//       PerformanceManager::CallOnGraph(FROM_HERE,
//           base::BindOnce(&Start, base::Unretained(this)));
//     }
//
//     void Start(Graph* graph) {
//       DCHECK_ON_GRAPH_SEQUENCE(graph);
//
//       // Creating a MeasurementRequest with the |graph| parameter
//       // automatically starts measurements.
//       request_ =
//           std::make_unique<V8PerFrameMemoryDecorator::MeasurementRequest>(
//               base::TimeDelta::FromSeconds(30), graph);
//       observer_ = std::make_unique<Observer>();
//       V8PerFrameMemoryDecorator::GetFromGraph(graph)->AddObserver(
//           observer_.get());
//     }
//
//     void Stop(Graph* graph) {
//       DCHECK_ON_GRAPH_SEQUENCE(graph);
//
//       // |observer_| can be deleted any time after calling RemoveObserver.
//       V8PerFrameMemoryDecorator::GetFromGraph(graph)->RemoveObserver(
//           observer_.get());
//
//       // Measurements stop when |request_| is deleted.
//       request_.reset();
//       observer_.reset();
//     }
//
//    private:
//     std::unique_ptr<V8PerFrameMemoryDecorator::MeasurementRequest> request_;
//     std::unique_ptr<Observer> observer_;
//   };
//
// Same, but from the another thread:
//
//   class Observer : public V8PerFrameMemoryObserverAnySeq {
//    public:
//     // Called on the same sequence for each process.
//     void OnV8MemoryMeasurementAvailable(
//         RenderProcessHostId process_id,
//         const V8PerFrameMemoryDecorator::ProcessData& process_data,
//         const V8PerFrameMemoryObserverAnySeq::FrameDataMap& frame_data)
//         override {
//       const auto* process = RenderProcessHost::FromID(process_id.value());
//       if (!process) {
//         // Process was deleted after measurement arrived on the PM sequence.
//         return;
//       }
//       LOG(INFO) << "Process " << process->GetID() <<
//           " reported " << process_data.unassociated_v8_bytes_used() <<
//           " bytes of V8 memory that wasn't associated with a frame.";
//       for (std::pair<
//             content::GlobalFrameRoutingId,
//             V8PerFrameMemoryDecorator::FrameData
//           > frame_and_data : frame_data) {
//         const auto* frame = RenderFrameHost::FromID(frame_and_data.first);
//         if (!frame) {
//           // Frame was deleted after measurement arrived on the PM sequence.
//           continue;
//         }
//         LOG(INFO) << "Frame " << frame->GetFrameToken() <<
//             " using " << token_and_data.second.v8_bytes_used() <<
//             " bytes of V8 memory in its main world.";
//       }
//     }
//   };
//
//  class MemoryMonitor {
//    public:
//     MemoryMonitor() {
//       Start();
//     }
//
//     void Start() {
//       DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
//
//       // Creating a MeasurementRequest with the |graph| parameter
//       // automatically starts measurements.
//       request_ = std::make_unique<V8PerFrameMemoryRequestAnySeq>(
//           base::TimeDelta::FromMinutes(2));
//       observer_ = std::make_unique<Observer>();
//       request_->AddObserver(observer_.get());
//     }
//
//     void Stop() {
//       DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
//
//       // |observer_| must be removed from |request_| before deleting it.
//       // Afterwards they can be deleted in any order.
//       request_->RemoveObserver(observer_.get());
//       observer_.reset();
//
//       // Measurements stop when |request_| is deleted.
//       request_.reset();
//     }
//
//    private:
//     std::unique_ptr<V8PerFrameMemoryRequestAnySeq> request_;
//     std::unique_ptr<Observer> observer_;
//
//     SEQUENCE_CHECKER(sequence_checker_);
//   };

class V8PerFrameMemoryDecorator
    : public GraphOwned,
      public GraphRegisteredImpl<V8PerFrameMemoryDecorator>,
      public ProcessNode::ObserverDefaultImpl,
      public NodeDataDescriberDefaultImpl {
 public:
  class MeasurementRequest;
  class FrameData;
  class ProcessData;
  class Observer;

  // Internal helper class that can call NotifyObserversOnMeasurementAvailable.
  class ObserverNotifier;

  V8PerFrameMemoryDecorator();
  ~V8PerFrameMemoryDecorator() override;

  V8PerFrameMemoryDecorator(const V8PerFrameMemoryDecorator&) = delete;
  V8PerFrameMemoryDecorator& operator=(const V8PerFrameMemoryDecorator&) =
      delete;

  // GraphOwned implementation.
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // ProcessNodeObserver overrides.
  void OnProcessNodeAdded(const ProcessNode* process_node) override;

  // NodeDataDescriber overrides.
  base::Value DescribeFrameNodeData(const FrameNode* node) const override;
  base::Value DescribeProcessNodeData(const ProcessNode* node) const override;

  // Returns the amount of time to wait between requests for each process.
  // Returns a zero TimeDelta if no requests should be made.
  base::TimeDelta GetMinTimeBetweenRequestsPerProcess() const;

  // Adds/removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // TODO(b/1080672): Use the PassKey pattern instead of these friend
  // statements.

  // MeasurementRequest calls AddMeasurementRequest and
  // RemoveMeasurementRequest.
  friend class MeasurementRequest;

  // ObserverNotifier calls NotifyObserversOnMeasurementAvailable
  friend class ObserverNotifier;

  void AddMeasurementRequest(MeasurementRequest* request);
  void RemoveMeasurementRequest(MeasurementRequest* request);
  void UpdateProcessMeasurementSchedules() const;

  // Invoked by ObserverNotifier when a measurement is received.
  void NotifyObserversOnMeasurementAvailable(
      const ProcessNode* const process_node) const;

  Graph* graph_ = nullptr;

  // List of requests sorted by sample_frequency (lowest first).
  std::vector<MeasurementRequest*> measurement_requests_;

  // TODO(b/1080672): Move the ObserverList into MeasurementRequest, so that
  // the lifetime of the observers aren't tied to the decorator, and add
  // check_empty=true.
  base::ObserverList<Observer> observers_;

  SEQUENCE_CHECKER(sequence_checker_);
};

class V8PerFrameMemoryRequestAnySeq;

class V8PerFrameMemoryDecorator::MeasurementRequest {
 public:
  // Creates a MeasurementRequest but does not start the measurements. Call
  // StartMeasurement to add it to the request list.
  explicit MeasurementRequest(const base::TimeDelta& sample_frequency);

  // Creates a MeasurementRequest and calls StartMeasurement. This will request
  // measurements for all ProcessNode's in |graph| with frequency
  // |sample_frequency|.
  MeasurementRequest(const base::TimeDelta& sample_frequency, Graph* graph);
  ~MeasurementRequest();

  MeasurementRequest(const MeasurementRequest&) = delete;
  MeasurementRequest& operator=(const MeasurementRequest&) = delete;

  const base::TimeDelta& sample_frequency() const { return sample_frequency_; }

  // Requests measurements for all ProcessNode's in |graph| with this object's
  // sample frequency. This must only be called once for each
  // MeasurementRequest.
  void StartMeasurement(Graph* graph);

 private:
  // V8PerFrameMemoryDecorator calls OnDecoratorUnregistered.
  friend class V8PerFrameMemoryDecorator;

  // V8PerFrameMemoryRequestAnySeq calls StartMeasurementFromOffSequence.
  friend class V8PerFrameMemoryRequestAnySeq;

  // Private constructor for V8PerFrameMemoryRequestAnySeq. Saves
  // |off_sequence_request| as a pointer to the off-sequence object that
  // triggered the request and starts measurements with frequency
  // |sample_frequency|.
  MeasurementRequest(
      const base::TimeDelta& sample_frequency,
      base::WeakPtr<V8PerFrameMemoryRequestAnySeq> off_sequence_request);

  void OnDecoratorUnregistered();

  base::TimeDelta sample_frequency_;
  V8PerFrameMemoryDecorator* decorator_ = nullptr;

  // Pointer back to the off-sequence V8PerFrameMemoryRequestAnySeq that
  // created this, if any.
  base::WeakPtr<V8PerFrameMemoryRequestAnySeq> off_sequence_request_;

  // Sequence that |off_sequence_request_| lives on.
  scoped_refptr<base::SequencedTaskRunner> off_sequence_request_sequence_;

  SEQUENCE_CHECKER(sequence_checker_);
};

class V8PerFrameMemoryDecorator::FrameData {
 public:
  FrameData() = default;
  virtual ~FrameData() = default;

  bool operator==(const FrameData& other) const {
    return v8_bytes_used_ == other.v8_bytes_used_;
  }

  // Returns the number of bytes used by V8 for this frame at the last
  // measurement.
  uint64_t v8_bytes_used() const { return v8_bytes_used_; }

  void set_v8_bytes_used(uint64_t v8_bytes_used) {
    v8_bytes_used_ = v8_bytes_used;
  }

  // Returns FrameData for the given node, or nullptr if no measurement has
  // been taken. The returned pointer must only be accessed on the graph
  // sequence and may go invalid at any time after leaving the calling scope.
  static const FrameData* ForFrameNode(const FrameNode* node);

 private:
  uint64_t v8_bytes_used_ = 0;
};

class V8PerFrameMemoryDecorator::ProcessData {
 public:
  ProcessData() = default;
  virtual ~ProcessData() = default;

  bool operator==(const ProcessData& other) const {
    return unassociated_v8_bytes_used_ == other.unassociated_v8_bytes_used_;
  }

  // Returns the number of bytes used by V8 at the last measurement in this
  // process that could not be attributed to a frame.
  uint64_t unassociated_v8_bytes_used() const {
    return unassociated_v8_bytes_used_;
  }

  void set_unassociated_v8_bytes_used(uint64_t unassociated_v8_bytes_used) {
    unassociated_v8_bytes_used_ = unassociated_v8_bytes_used;
  }

  // Returns FrameData for the given node, or nullptr if no measurement has
  // been taken. The returned pointer must only be accessed on the graph
  // sequence and may go invalid at any time after leaving the calling scope.
  static const ProcessData* ForProcessNode(const ProcessNode* node);

 private:
  uint64_t unassociated_v8_bytes_used_ = 0;
};

class V8PerFrameMemoryDecorator::Observer : public base::CheckedObserver {
 public:
  // Called on the PM sequence when a measurement is available for
  // |process_node|. The measurements can be read by walking the graph from
  // |process_node| to find frame nodes, and calling
  // ProcessData::ForProcessNode and FrameData::ForFrameNode to retrieve the
  // measurement data.
  virtual void OnV8MemoryMeasurementAvailable(
      const ProcessNode* const process_node) = 0;
};

// Observer that can be created on any sequence, and will be notified on that
// sequence when measurements are available. Register the observer through
// V8PerFrameMemoryRequestAnySeq::AddObserver. The
// V8PerFrameMemoryRequestAnySeq must live on the same sequence as the
// observer.
class V8PerFrameMemoryObserverAnySeq : public base::CheckedObserver {
 public:
  // TODO(crbug.com/1096617): Should use FrameToken here instead of routing id.
  using FrameDataMap = base::flat_map<content::GlobalFrameRoutingId,
                                      V8PerFrameMemoryDecorator::FrameData>;

  // Called on the observer's sequence when a measurement is available for the
  // process with ID |render_process_host_id|. The notification includes the
  // measurement data for the process and each frame that had a result in that
  // process at the time of the measurement, so that the implementer doesn't
  // need to return to the PM sequence to read it.
  virtual void OnV8MemoryMeasurementAvailable(
      RenderProcessHostId render_process_host_id,
      const V8PerFrameMemoryDecorator::ProcessData& process_data,
      const FrameDataMap& frame_data) = 0;
};

// Wrapper that can instantiate a V8PerFrameMemoryDecorator::MeasurementRequest
// from any sequence.
class V8PerFrameMemoryRequestAnySeq {
 public:
  explicit V8PerFrameMemoryRequestAnySeq(
      const base::TimeDelta& sample_frequency);
  ~V8PerFrameMemoryRequestAnySeq();

  V8PerFrameMemoryRequestAnySeq(const V8PerFrameMemoryRequestAnySeq&) = delete;
  V8PerFrameMemoryRequestAnySeq& operator=(
      const V8PerFrameMemoryRequestAnySeq&) = delete;

  // Adds an observer that was created on the same sequence as the
  // V8PerFrameMemoryRequestAnySeq.
  void AddObserver(V8PerFrameMemoryObserverAnySeq* observer);

  // Removes an observer that was added with AddObserver.
  void RemoveObserver(V8PerFrameMemoryObserverAnySeq* observer);

 private:
  // V8PerFrameMemoryDecorator calls NotifyObserversOnMeasurementAvailable.
  friend class V8PerFrameMemoryDecorator;

  void NotifyObserversOnMeasurementAvailable(
      RenderProcessHostId render_process_host_id,
      const V8PerFrameMemoryDecorator::ProcessData& process_data,
      const V8PerFrameMemoryObserverAnySeq::FrameDataMap& frame_data) const;

  std::unique_ptr<V8PerFrameMemoryDecorator::MeasurementRequest> request_;
  base::ObserverList<V8PerFrameMemoryObserverAnySeq, /*check_empty=*/true>
      observers_;

  // This object can live on any sequence but all methods and the destructor
  // must be called from that sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<V8PerFrameMemoryRequestAnySeq> weak_factory_{this};
};

namespace internal {

// A callback that will bind a V8PerFrameMemoryReporter interface to
// communicate with the given process. Exposed so that it can be overridden to
// implement the interface with a test fake.
using BindV8PerFrameMemoryReporterCallback = base::RepeatingCallback<void(
    mojo::PendingReceiver<performance_manager::mojom::V8PerFrameMemoryReporter>,
    RenderProcessHostProxy)>;

// Sets a callback that will be used to bind the V8PerFrameMemoryReporter
// interface. The callback is owned by the caller and must live until this
// function is called again with nullptr.
void SetBindV8PerFrameMemoryReporterCallbackForTesting(
    BindV8PerFrameMemoryReporterCallback* callback);

}  // namespace internal

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_V8_PER_FRAME_MEMORY_DECORATOR_H_
