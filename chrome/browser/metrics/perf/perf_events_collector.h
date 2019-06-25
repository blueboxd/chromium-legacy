// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PERF_PERF_EVENTS_COLLECTOR_H_
#define CHROME_BROWSER_METRICS_PERF_PERF_EVENTS_COLLECTOR_H_

#include <map>

#include "chrome/browser/metrics/perf/metric_collector.h"
#include "chrome/browser/metrics/perf/random_selector.h"

namespace base {
class SequencedTaskRunner;
}

namespace metrics {

struct CPUIdentity;
class PerfOutputCall;
class WindowedIncognitoObserver;

// Enables collection of perf events profile data. perf aka "perf events" is a
// performance profiling infrastructure built into the linux kernel. For more
// information, see: https://perf.wiki.kernel.org/index.php/Main_Page.
class PerfCollector : public MetricCollector {
 public:
  PerfCollector();
  ~PerfCollector() override;

  void Init() override;

 protected:
  // Returns the perf proto type associated with the given vector of perf
  // arguments, starting with "perf" itself in |args[0]|.
  static PerfProtoType GetPerfProtoType(const std::vector<std::string>& args);

  void OnPerfOutputComplete(
      std::unique_ptr<WindowedIncognitoObserver> incognito_observer,
      std::unique_ptr<SampledProfile> sampled_profile,
      PerfProtoType type,
      bool has_cycles,
      std::string perf_stdout);

  // Parses a PerfDataProto or PerfStatProto from serialized data |perf_stdout|,
  // if non-empty. Which proto to use depends on |subcommand|. If |perf_stdout|
  // is empty, it is counted as an error. |incognito_observer| indicates
  // whether an incognito window had been opened during the profile collection
  // period. If there was an incognito window, discard the incoming data.
  void ParseOutputProtoIfValid(
      std::unique_ptr<WindowedIncognitoObserver> incognito_observer,
      std::unique_ptr<SampledProfile> sampled_profile,
      PerfProtoType type,
      bool has_cycles,
      const std::string& perf_stdout);

  // MetricCollector:
  base::WeakPtr<MetricCollector> GetWeakPtr() override;
  bool ShouldCollect() const override;
  void CollectProfile(std::unique_ptr<SampledProfile> sampled_profile) override;

  const RandomSelector& command_selector() const { return command_selector_; }

  // Executes asynchronously on another thread pool. When it finishes, posts a
  // task on the given task_runner.
  static void ParseCPUFrequencies(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::WeakPtr<PerfCollector> perf_collector);
  // Executes on the same sequence as the processing of perf data.
  void SaveCPUFrequencies(const std::vector<uint32_t>& frequencies);

  // Enumeration representing success and various failure modes for parsing CPU
  // frequencies. These values are persisted to logs. Entries should not be
  // renumbered and numeric values should never be reused.
  enum class ParseFrequencyStatus {
    kSuccess,
    kNumCPUsIsZero,
    kSomeZeroCPUFrequencies,
    kAllZeroCPUFrequencies,
    // Magic constant used by the histogram macros.
    kMaxValue = kAllZeroCPUFrequencies,
  };

  // Vector of max frequencies associated with each logical CPU. Computed
  // asynchronously at start, but initialized using the same sequence as the
  // perf data processing code.
  std::vector<uint32_t> max_frequencies_mhz_;

 private:
  // Change the values in |collection_params_| and the commands in
  // |command_selector_| for any keys that are present in |params|.
  void SetCollectionParamsFromVariationParams(
      const std::map<std::string, std::string>& params);

  // Set of commands to choose from.
  RandomSelector command_selector_;

  // An active call to perf/quipper, if set.
  std::unique_ptr<PerfOutputCall> perf_output_call_;

  base::WeakPtrFactory<PerfCollector> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PerfCollector);
};

// Exposed for unit testing.
namespace internal {

// Return the default set of perf commands and their odds of selection given
// the identity of the CPU in |cpuid|.
std::vector<RandomSelector::WeightAndValue> GetDefaultCommandsForCpu(
    const CPUIdentity& cpuid);

// For the "PerfCommand::"-prefixed keys in |params|, return the cpu specifier
// that is the narrowest match for the CPU identified by |cpuid|.
// Valid CPU specifiers, in increasing order of specificity, are:
// "default", a system architecture (e.g. "x86_64"), a CPU microarchitecture
// (currently only some Intel and AMD uarchs supported), or a CPU model name
// substring.
std::string FindBestCpuSpecifierFromParams(
    const std::map<std::string, std::string>& params,
    const CPUIdentity& cpuid);

// Returns if the given perf command samples CPU cycles.
bool CommandSamplesCPUCycles(const std::vector<std::string>& args);

}  // namespace internal

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_PERF_PERF_EVENTS_COLLECTOR_H_
