// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CALL_STACK_PROFILE_BUILDER_H_
#define COMPONENTS_METRICS_CALL_STACK_PROFILE_BUILDER_H_

#include <limits>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/profiler/metadata_recorder.h"
#include "base/profiler/profile_builder.h"
#include "base/sampling_heap_profiler/module_cache.h"
#include "base/time/time.h"
#include "components/metrics/call_stack_profile_params.h"
#include "components/metrics/child_call_stack_profile_collector.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

// Interface that allows the CallStackProfileBuilder to provide ids for distinct
// work items. Samples with the same id are tagged as coming from the same work
// item in the recorded samples.
class WorkIdRecorder {
 public:
  WorkIdRecorder() = default;
  virtual ~WorkIdRecorder() = default;

  // This function is invoked on the profiler thread while the target thread is
  // suspended so must not take any locks, including indirectly through use of
  // heap allocation, LOG, CHECK, or DCHECK.
  virtual unsigned int RecordWorkId() const = 0;

  WorkIdRecorder(const WorkIdRecorder&) = delete;
  WorkIdRecorder& operator=(const WorkIdRecorder&) = delete;
};

// An instance of the class is meant to be passed to base::StackSamplingProfiler
// to collect profiles. The profiles collected are uploaded via the metrics log.
//
// This uses the new StackSample encoding rather than the legacy Sample
// encoding.
class CallStackProfileBuilder : public base::ProfileBuilder {
 public:
  // |completed_callback| is made when sampling a profile completes. Other
  // threads, including the UI thread, may block on callback completion so this
  // should run as quickly as possible.
  //
  // IMPORTANT NOTE: The callback is invoked on a thread the profiler
  // constructs, rather than on the thread used to construct the profiler, and
  // thus the callback must be callable on any thread.
  explicit CallStackProfileBuilder(
      const CallStackProfileParams& profile_params,
      const WorkIdRecorder* work_id_recorder = nullptr,
      base::OnceClosure completed_callback = base::OnceClosure());

  ~CallStackProfileBuilder() override;

  // Both weight and count are used by the heap profiler only.
  void OnSampleCompleted(std::vector<base::Frame> frames,
                         size_t weight,
                         size_t count);

  // base::ProfileBuilder:
  base::ModuleCache* GetModuleCache() override;
  void RecordMetadata(
      base::ProfileBuilder::MetadataProvider* metadata_provider) override;
  void OnSampleCompleted(std::vector<base::Frame> frames) override;
  void OnProfileCompleted(base::TimeDelta profile_duration,
                          base::TimeDelta sampling_period) override;

  // Sets the callback to use for reporting browser process profiles. This
  // indirection is required to avoid a dependency on unnecessary metrics code
  // in child processes.
  static void SetBrowserProcessReceiverCallback(
      const base::RepeatingCallback<void(base::TimeTicks, SampledProfile)>&
          callback);

  // Sets the CallStackProfileCollector interface from |browser_interface|.
  // This function must be called within child processes.
  static void SetParentProfileCollectorForChildProcess(
      mojo::PendingRemote<metrics::mojom::CallStackProfileCollector>
          browser_interface);

 protected:
  // Test seam.
  virtual void PassProfilesToMetricsProvider(SampledProfile sampled_profile);

 private:
  // The functor for Stack comparison.
  struct StackComparer {
    bool operator()(const CallStackProfile::Stack* stack1,
                    const CallStackProfile::Stack* stack2) const;
  };

  // Comparison function for the metadata map.
  struct MetadataKey;
  struct MetadataKeyCompare {
    bool operator()(const MetadataKey& a, const MetadataKey& b) const;
  };

  // Definitions for a map-based representation of sample metadata.
  struct MetadataKey {
    MetadataKey(uint64_t name_hash, base::Optional<int64_t> key);

    MetadataKey(const MetadataKey& other);
    MetadataKey& operator=(const MetadataKey& other);

    // The name_hash and optional user-specified key uniquely identifies a
    // metadata value. See base::MetadataRecorder for details.
    uint64_t name_hash;
    base::Optional<int64_t> key;
  };
  using MetadataMap = std::map<MetadataKey, int64_t, MetadataKeyCompare>;

  // Creates the metdata map from the array of items.
  MetadataMap CreateMetadataMap(base::ProfileBuilder::MetadataItemArray items,
                                size_t item_count);

  // Returns all metadata items with new values in the current sample.
  MetadataMap GetNewOrModifiedMetadataItems(const MetadataMap& current_items,
                                            const MetadataMap& previous_items);

  // Returns all metadata items deleted since the previous sample.
  MetadataMap GetDeletedMetadataItems(const MetadataMap& current_items,
                                      const MetadataMap& previous_items);

  // Creates MetadataItems for the currently active metadata, adding new name
  // hashes to |metadata_name_hashes| if necessary. The same
  // |metadata_name_hashes| must be passed to each invocation, and must not be
  // modified outside this function.
  google::protobuf::RepeatedPtrField<CallStackProfile::MetadataItem>
  CreateSampleMetadata(
      google::protobuf::RepeatedField<uint64_t>* metadata_name_hashes);

  // Appends the |name_hash| to |name_hashes| if it's not already
  // present. Returns its index in |name_hashes|.
  size_t MaybeAppendNameHash(
      uint64_t name_hash,
      google::protobuf::RepeatedField<uint64_t>* metadata_name_hashes);

  // The module cache to use for the duration the sampling associated with this
  // ProfileBuilder.
  base::ModuleCache module_cache_;

  unsigned int last_work_id_ = std::numeric_limits<unsigned int>::max();
  bool is_continued_work_ = false;
  const WorkIdRecorder* const work_id_recorder_;

  // The SampledProfile protobuf message which contains the collected stack
  // samples.
  SampledProfile sampled_profile_;

  // The indexes of stacks, indexed by stack's address.
  std::map<const CallStackProfile::Stack*, int, StackComparer> stack_index_;

  // The indexes of modules in the modules_ vector below..
  std::unordered_map<const base::ModuleCache::Module*, size_t> module_index_;

  // The distinct modules in the current profile.
  std::vector<const base::ModuleCache::Module*> modules_;

  // Callback made when sampling a profile completes.
  base::OnceClosure completed_callback_;

  // The start time of a profile collection.
  const base::TimeTicks profile_start_time_;

  // The data fetched from the MetadataRecorder for the next sample.
  base::ProfileBuilder::MetadataItemArray metadata_items_;
  size_t metadata_item_count_ = 0;

  // The data fetched from the MetadataRecorder for the previous sample.
  MetadataMap previous_items_;

  // Maps metadata hash to index in |metadata_name_hash| array.
  std::unordered_map<uint64_t, int> metadata_hashes_cache_;

  DISALLOW_COPY_AND_ASSIGN(CallStackProfileBuilder);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CALL_STACK_PROFILE_BUILDER_H_
