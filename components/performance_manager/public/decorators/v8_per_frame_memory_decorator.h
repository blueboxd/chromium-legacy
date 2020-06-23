// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_V8_PER_FRAME_MEMORY_DECORATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_V8_PER_FRAME_MEMORY_DECORATOR_H_

#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "content/public/common/performance_manager/v8_per_frame_memory.mojom.h"

namespace performance_manager {

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
// MeasurementRequest, FrameData and ProcessData must all be accessed on the
// graph sequence.
class V8PerFrameMemoryDecorator
    : public GraphOwned,
      public GraphRegisteredImpl<V8PerFrameMemoryDecorator>,
      public ProcessNode::ObserverDefaultImpl,
      public NodeDataDescriberDefaultImpl {
 public:
  class MeasurementRequest;
  class FrameData;
  class ProcessData;

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

 private:
  friend class MeasurementRequest;

  void AddMeasurementRequest(MeasurementRequest* request);
  void RemoveMeasurementRequest(MeasurementRequest* request);
  void UpdateProcessMeasurementSchedules() const;

  Graph* graph_ = nullptr;

  // List of requests sorted by sample_frequency (lowest first).
  std::vector<MeasurementRequest*> measurement_requests_;

  SEQUENCE_CHECKER(sequence_checker_);
};

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
  friend class V8PerFrameMemoryDecorator;
  void OnDecoratorUnregistered();

  base::TimeDelta sample_frequency_;
  V8PerFrameMemoryDecorator* decorator_ = nullptr;
};

class V8PerFrameMemoryDecorator::FrameData {
 public:
  FrameData() = default;
  virtual ~FrameData() = default;

  FrameData(const FrameData&) = delete;
  FrameData& operator=(const FrameData&) = delete;

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

  ProcessData(const ProcessData&) = delete;
  ProcessData& operator=(const ProcessData&) = delete;

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

// Wrapper that can instantiate a V8PerFrameMemoryDecorator::MeasurementRequest
// from any sequence.
class V8PerFrameMemoryRequest {
 public:
  explicit V8PerFrameMemoryRequest(const base::TimeDelta& sample_frequency);
  ~V8PerFrameMemoryRequest();

  V8PerFrameMemoryRequest(const V8PerFrameMemoryRequest&) = delete;
  V8PerFrameMemoryRequest& operator=(const V8PerFrameMemoryRequest&) = delete;

 private:
  std::unique_ptr<V8PerFrameMemoryDecorator::MeasurementRequest> request_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_V8_PER_FRAME_MEMORY_DECORATOR_H_
