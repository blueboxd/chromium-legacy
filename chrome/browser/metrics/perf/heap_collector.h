// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PERF_HEAP_COLLECTOR_H_
#define CHROME_BROWSER_METRICS_PERF_HEAP_COLLECTOR_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/optional.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "chrome/browser/metrics/perf/metric_collector.h"
#include "chrome/browser/metrics/perf/perf_output.h"
#include "chrome/browser/ui/browser_list_observer.h"

class Browser;

namespace base {
class CommandLine;
class File;
class FilePath;
}  // namespace base

namespace metrics {

// Collection mode type.
enum class HeapCollectionMode {
  // No heap collection.
  kNone = 0,
  // Use allocation sampling inside tcmalloc.
  kTcmalloc = 1,
  // Use allocation sampling at the shim layer.
  kShimLayer = 2,
};

// Enables collection of heap profiles using the tcmalloc heap sampling
// profiler.
class HeapCollector : public MetricCollector, public BrowserListObserver {
 public:
  explicit HeapCollector(HeapCollectionMode mode);
  ~HeapCollector() override;

  // MetricCollector:
  void Init() override;

  static HeapCollectionMode CollectionModeFromString(std::string mode);

 protected:
  // MetricCollector:
  base::WeakPtr<MetricCollector> GetWeakPtr() override;
  bool ShouldCollect() const override;
  void CollectProfile(std::unique_ptr<SampledProfile> sampled_profile) override;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // Fetches a heap profile from tcmalloc, dumps it to a temp file, and returns
  // the path.
  base::Optional<base::FilePath> DumpProfileToTempFile();

  // Generates a quipper command to parse the given profile file.
  base::CommandLine MakeQuipperCommand(const base::FilePath& profile_path);

  // Executes the given command line to parse a profile stored at the given
  // path and saves it in the given sampled profile. The given temporary profile
  // file is removed after parsing.
  void ParseAndSaveProfile(const base::CommandLine& parser,
                           const base::FilePath& profile_path,
                           std::unique_ptr<SampledProfile> sampled_profile);

  // Start and stop the collection.
  void EnableSampling();
  void DisableSampling();
  HeapCollectionMode Mode() const { return mode_; }
  bool IsEnabled() const { return is_enabled_; }

 private:
  // Change the values in |collection_params_| based on the values of field
  // trial parameters.
  void SetCollectionParamsFromFeatureParams();

  // Heap sampling period.
  size_t sampling_period_bytes_;

  // Heap collection mode.
  HeapCollectionMode mode_;

  // The collector state.
  bool is_enabled_;

  base::WeakPtrFactory<HeapCollector> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HeapCollector);
};

// Exposed for unit testing.
namespace internal {

// Writes the given heap samples and runtime mappings to the given output file
// in the same format as the one produced by the tcmalloc sampler.
bool WriteHeapProfileToFile(
    base::File* out,
    const std::vector<base::SamplingHeapProfiler::Sample>& samples,
    const std::string& proc_maps);

}  // namespace internal

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_PERF_HEAP_COLLECTOR_H_
