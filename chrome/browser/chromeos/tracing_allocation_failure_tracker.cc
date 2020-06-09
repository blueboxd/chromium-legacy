// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/tracing_allocation_failure_tracker.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/chromeos/external_metrics.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/crash/core/app/crashpad.h"
#include "components/metrics/serialization/metric_sample.h"
#include "components/metrics/serialization/serialization_utils.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"

namespace chromeos {

namespace {

constexpr char kCrashHandlerMetricName[] =
    "CrashReport.DumpWithoutCrashingHandler.FromInitSharedMemoryIfNeeded";
// Crash handler that might handle base::debug::DumpWithoutCrashing.
// TODO(crbug.com/1074115): Remove once crbug.com/1074115 is resolved.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CrashHandler {
  kCrashpad = 0,
  kBreakpad = 1,
  kMaxValue = kBreakpad,
};

// UMA that records the return value of base::debug::DumpWithoutCrashing.
// TODO(crbug.com/1074115): Remove once crbug.com/1074115 is resolved.
constexpr char kDumpWithoutCrashingResultMetricName[] =
    "CrashReport.DumpWithoutCrashingResult.FromInitSharedMemoryIfNeeded2";
// Results of the DumpWithoutCrashing call inside
// tracing::ProducerClient::InitSharedMemoryIfNeeded, broken out by which crash
// handling system should have processed the DumpWithoutCrashing.
// TODO(crbug.com/1074115): Remove once crbug.com/1074115 is resolved.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DumpWithoutCrashingResult {
  kFailureCrashpad = 0,  // Crashpad was running and DumpWithoutCrashing
                         // returned false.
  kSuccessCrashpad = 1,  // Crashpad was running and DumpWithoutCrashing
                         // returned true.
  kFailureBreakpad = 2,  // Breakpad was running and DumpWithoutCrashing
                         // returned false.
  kSuccessBreakpad = 3,  // Breakpad was running and DumpWithoutCrashing
                         // returned true.
  kMaxValue = kSuccessBreakpad,
};

// Local wrapper around GetCollectStatsConsent() to make our collection even
// more restrictive.
bool ShouldCollectStats() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return GoogleUpdateSettings::GetCollectStatsConsent() &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             chromeos::switches::kGuestSession);
#else
  return false;
#endif
}

// TracingBufferAllocationFailureCallback and
// TracingBufferAllocationFailurePostDumpCallback are part of an investigation
// into why Breakpad doesn't seem to generate reports on some ChromeOS boards.
// See crbug.com/1074115 for the original bug.
//
// They are callbacks to work around issues getting UMA metrics in the failure
// case we are investigating. The specific issue is that the failing function
// (ProducerClient::InitSharedMemoryIfNeeded) is called before metrics
// persistence is set up, and the issue that is causing buffer allocation
// failures is also preventing us from setting up metrics persistence. In
// particular, on eve boards, we see multiple dump-without-crashing crashes
// being reported, but the metrics that were being recorded via
// UmaHistogramEnumeration before the DumpWithoutCrashing call were never being
// reported back.
//
// These callbacks bypass the normal metrics collection system and instead
// write to the uma-events file that ChromeOS programs use to communicate with
// Chrome (see
// https://chromium.googlesource.com/chromiumos/platform2/+/refs/heads/master/metrics/metrics_library.cc)
// This is a ChromeOS-specific workaround, which is why this code is inside
// chrome/browser/chromeos.
void TracingBufferAllocationFailureCallback() {
  if (!ShouldCollectStats()) {
    return;
  }

  CrashHandler handler = crash_reporter::IsCrashpadEnabled()
                             ? CrashHandler::kCrashpad
                             : CrashHandler::kBreakpad;
  // Note: The parameter in LinearHistogramSample() is called max, but every
  // single usage I can find in the ChromeOS codebase passes max value + 1,
  // so following the herd here.
  const int kNumValues = static_cast<int>(CrashHandler::kMaxValue) + 1;
  metrics::SerializationUtils::WriteMetricToFile(
      *metrics::MetricSample::LinearHistogramSample(
          kCrashHandlerMetricName, static_cast<int>(handler), kNumValues),
      chromeos::ExternalMetrics::kEventsFilePath);
}

void TracingBufferAllocationFailurePostDumpCallback(
    bool dump_without_crashing_result) {
  if (!ShouldCollectStats()) {
    return;
  }

  DumpWithoutCrashingResult result;
  if (crash_reporter::IsCrashpadEnabled()) {
    if (dump_without_crashing_result) {
      result = DumpWithoutCrashingResult::kSuccessCrashpad;
    } else {
      result = DumpWithoutCrashingResult::kFailureCrashpad;
    }
  } else {
    if (dump_without_crashing_result) {
      result = DumpWithoutCrashingResult::kSuccessBreakpad;
    } else {
      result = DumpWithoutCrashingResult::kFailureBreakpad;
    }
  }

  // Number of possible enum values. See above about passing in max vs. number
  // of values.
  const int kNumValues =
      static_cast<int>(DumpWithoutCrashingResult::kMaxValue) + 1;
  metrics::SerializationUtils::WriteMetricToFile(
      *metrics::MetricSample::LinearHistogramSample(
          kDumpWithoutCrashingResultMetricName, static_cast<int>(result),
          kNumValues),
      chromeos::ExternalMetrics::kEventsFilePath);
}

}  // namespace

void SetUpTracingAllocatorFailureTracker() {
  tracing::PerfettoTracedProcess* ptp = tracing::PerfettoTracedProcess::Get();
  ptp->producer_client()->SetBufferAllocationFailureCallbacks(
      base::Bind(&TracingBufferAllocationFailureCallback),
      base::Bind(&TracingBufferAllocationFailurePostDumpCallback));
}

}  // namespace chromeos
