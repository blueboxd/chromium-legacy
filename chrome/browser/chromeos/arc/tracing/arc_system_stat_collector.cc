// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/tracing/arc_system_stat_collector.h"

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/arc/tracing/arc_system_model.h"
#include "chrome/browser/chromeos/arc/tracing/arc_value_event_trimmer.h"

namespace arc {

namespace {

// Interval to update system stats.
constexpr base::TimeDelta kSystemStatUpdateInterval =
    base::TimeDelta::FromMilliseconds(10);

bool IsWhitespace(char c) {
  return c == ' ' || c == '\t' || c == '\n';
}

bool IsDigit(char c) {
  return c >= '0' && c <= '9';
}

bool IsEnd(char c) {
  return IsWhitespace(c) || c == 0;
}

// Detects path to stat file that contains temperature for CPU Core 0 that is
// used as temperature for CPU. Not all cores may be covered by this statistics.
// Detected path is stored |path_|
class CpuTemperaturePathDetector {
 public:
  CpuTemperaturePathDetector() {
    base::FileEnumerator hwmon_enumerator(
        base::FilePath(FILE_PATH_LITERAL("/sys/class/hwmon/")),
        false /* recursive */, base::FileEnumerator::DIRECTORIES,
        FILE_PATH_LITERAL("hwmon*"));
    for (base::FilePath hwmon_path = hwmon_enumerator.Next();
         !hwmon_path.empty(); hwmon_path = hwmon_enumerator.Next()) {
      base::FileEnumerator enumerator(
          hwmon_path, false, base::FileEnumerator::FILES, "temp*_label");
      for (base::FilePath temperature_label_path = enumerator.Next();
           !temperature_label_path.empty();
           temperature_label_path = enumerator.Next()) {
        std::string label;
        if (!base::ReadFileToString(temperature_label_path, &label))
          continue;
        base::TrimWhitespaceASCII(label, base::TRIM_TRAILING, &label);
        if (label != "Core 0" && label != "Physical id 0")
          continue;
        std::string temperature_input_path_string =
            temperature_label_path.value();
        base::ReplaceSubstringsAfterOffset(&temperature_input_path_string, 0,
                                           "label", "input");
        const base::FilePath temperature_input_path =
            base::FilePath(temperature_input_path_string);
        if (!base::PathExists(temperature_input_path))
          continue;
        path_ = temperature_input_path;
        VLOG(1) << "Detected path to read CPU temperature (" << label
                << "): " << temperature_input_path;
        return;
      }
    }
    LOG(WARNING) << "Not detected path to read CPU temperature.";
  }

  const base::FilePath& path() const { return path_; }

 private:
  base::FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(CpuTemperaturePathDetector);
};

const base::FilePath& GetCpuTemperaturePathOnFileThread() {
  static base::NoDestructor<CpuTemperaturePathDetector> instance;
  return instance->path();
}

}  // namespace

struct ArcSystemStatCollector::Sample {
  base::TimeTicks timestamp;
  int swap_sectors_read = 0;
  int swap_sectors_write = 0;
  int swap_waiting_time_ms = 0;
  int mem_total_kb = 0;
  int mem_used_kb = 0;
  int gem_objects = 0;
  int gem_size_kb = 0;
  int cpu_temperature = std::numeric_limits<int>::min();
};

// static
constexpr int ArcSystemStatCollector::kZramStatColumns[];

// static
constexpr int ArcSystemStatCollector::kMemInfoColumns[];

// static
constexpr int ArcSystemStatCollector::kGemInfoColumns[];

// static
constexpr int ArcSystemStatCollector::kCpuTempInfoColumns[];

ArcSystemStatCollector::ArcSystemStatCollector() {}

ArcSystemStatCollector::~ArcSystemStatCollector() = default;

void ArcSystemStatCollector::Start(const base::TimeDelta& max_interval) {
  const size_t sample_count =
      1 + max_interval.InMicroseconds() /
              kSystemStatUpdateInterval.InMicroseconds();
  samples_.resize(sample_count);
  write_index_ = 0;
  // Maximum 10 warning per session.
  missed_update_warning_left_ = 10;

  background_task_runner_ = base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  timer_.Start(
      FROM_HERE, kSystemStatUpdateInterval,
      base::BindRepeating(&ArcSystemStatCollector::ScheduleSystemStatUpdate,
                          base::Unretained(this)));
}

void ArcSystemStatCollector::Stop() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  background_task_runner_.reset();
  timer_.Stop();
  request_scheduled_ = false;
}

void ArcSystemStatCollector::Flush(const base::TimeTicks& min_timestamp,
                                   const base::TimeTicks& max_timestamp,
                                   ArcSystemModel* system_model) {
  DCHECK(!timer_.IsRunning());
  size_t sample_index =
      write_index_ >= samples_.size() ? write_index_ - samples_.size() : 0;
  ArcValueEventTrimmer mem_total(&system_model->memory_events(),
                                 ArcValueEvent::Type::kMemTotal);
  ArcValueEventTrimmer mem_used(&system_model->memory_events(),
                                ArcValueEvent::Type::kMemUsed);
  ArcValueEventTrimmer gem_objects(&system_model->memory_events(),
                                   ArcValueEvent::Type::kGemObjects);
  ArcValueEventTrimmer gem_size(&system_model->memory_events(),
                                ArcValueEvent::Type::kGemSize);
  ArcValueEventTrimmer swap_read(&system_model->memory_events(),
                                 ArcValueEvent::Type::kSwapRead);
  ArcValueEventTrimmer swap_write(&system_model->memory_events(),
                                  ArcValueEvent::Type::kSwapWrite);
  ArcValueEventTrimmer swap_wait(&system_model->memory_events(),
                                 ArcValueEvent::Type::kSwapWait);
  ArcValueEventTrimmer cpu_temperature(&system_model->memory_events(),
                                       ArcValueEvent::Type::kCpuTemp);
  while (sample_index < write_index_) {
    const Sample& sample = samples_[sample_index % samples_.size()];
    ++sample_index;
    if (sample.timestamp > max_timestamp)
      break;
    if (sample.timestamp < min_timestamp)
      continue;
    const int64_t timestamp =
        (sample.timestamp - base::TimeTicks()).InMicroseconds();
    mem_total.MaybeAdd(timestamp, sample.mem_total_kb);
    mem_used.MaybeAdd(timestamp, sample.mem_used_kb);
    gem_objects.MaybeAdd(timestamp, sample.gem_objects);
    gem_size.MaybeAdd(timestamp, sample.gem_size_kb);
    swap_read.MaybeAdd(timestamp, sample.swap_sectors_read);
    swap_write.MaybeAdd(timestamp, sample.swap_sectors_write);
    swap_wait.MaybeAdd(timestamp, sample.swap_waiting_time_ms);
    if (sample.cpu_temperature > std::numeric_limits<int>::min())
      cpu_temperature.MaybeAdd(timestamp, sample.cpu_temperature);
  }
  // Trimmer may break time sequence for events of different types. However
  // time sequence of events of the same type should be preserved.
  std::sort(system_model->memory_events().begin(),
            system_model->memory_events().end(),
            [](const auto& lhs, const auto& rhs) {
              return lhs.timestamp < rhs.timestamp;
            });
}

void ArcSystemStatCollector::ScheduleSystemStatUpdate() {
  if (request_scheduled_) {
    if (missed_update_warning_left_-- > 0)
      LOG(WARNING) << "Dropping update, already pending";
    return;
  }
  base::TaskRunner* task_runner = background_task_runner_.get();
  base::PostTaskAndReplyWithResult(
      task_runner, FROM_HERE,
      base::BindOnce(&ArcSystemStatCollector::ReadSystemStatOnBackgroundThread),
      base::BindOnce(&ArcSystemStatCollector::UpdateSystemStatOnUiThread,
                     weak_ptr_factory_.GetWeakPtr()));
  request_scheduled_ = true;
}

// static
ArcSystemStatCollector::RuntimeFrame
ArcSystemStatCollector::ReadSystemStatOnBackgroundThread() {
  const base::FilePath zram_stat_path(
      FILE_PATH_LITERAL("/sys/block/zram0/stat"));
  const base::FilePath mem_info_path(FILE_PATH_LITERAL("/proc/meminfo"));

  ArcSystemStatCollector::RuntimeFrame current_frame;
  if (!ParseStatFile(zram_stat_path, kZramStatColumns,
                     current_frame.zram_stat)) {
    memset(current_frame.zram_stat, 0, sizeof(current_frame.zram_stat));
    static bool error_reported = false;
    if (!error_reported) {
      LOG(ERROR) << "Failed to read zram stat file: " << zram_stat_path.value();
      error_reported = true;
    }
  }

  if (!ParseStatFile(mem_info_path, kMemInfoColumns, current_frame.mem_info)) {
    memset(current_frame.mem_info, 0, sizeof(current_frame.mem_info));
    static bool error_reported = false;
    if (!error_reported) {
      LOG(ERROR) << "Failed to read mem info file: " << mem_info_path.value();
      error_reported = true;
    }
  }

#if defined(ARCH_CPU_ARM_FAMILY)
  const base::FilePath gem_info_path(
      FILE_PATH_LITERAL("/run/debugfs_gpu/exynos_gem_objects"));
#else
  const base::FilePath gem_info_path(
      FILE_PATH_LITERAL("/run/debugfs_gpu/i915_gem_objects"));
#endif
  if (!ParseStatFile(gem_info_path, kGemInfoColumns, current_frame.gem_info)) {
    memset(current_frame.gem_info, 0, sizeof(current_frame.gem_info));
    static bool error_reported = false;
    if (!error_reported) {
      LOG(ERROR) << "Failed to read gem info file: " << gem_info_path.value();
      error_reported = true;
    }
  }

  const base::FilePath& cpu_temp_path = GetCpuTemperaturePathOnFileThread();
  if (cpu_temp_path.empty() ||
      !ParseStatFile(cpu_temp_path, kCpuTempInfoColumns,
                     &current_frame.cpu_temperature_)) {
    current_frame.cpu_temperature_ = std::numeric_limits<int>::min();
  }

  return current_frame;
}

void ArcSystemStatCollector::UpdateSystemStatOnUiThread(
    RuntimeFrame current_frame) {
  DCHECK(request_scheduled_);
  request_scheduled_ = false;
  DCHECK(!samples_.empty());
  Sample& current_sample = samples_[write_index_ % samples_.size()];
  current_sample.timestamp = base::TimeTicks::Now();
  current_sample.mem_total_kb = current_frame.mem_info[0];
  // Total - available.
  current_sample.mem_used_kb =
      current_frame.mem_info[0] - current_frame.mem_info[1];
  current_sample.gem_objects = current_frame.gem_info[0];
  current_sample.gem_size_kb = current_frame.gem_info[1] / 1024;

  // We calculate delta, so ignore first update.
  if (write_index_) {
    current_sample.swap_sectors_read =
        current_frame.zram_stat[0] - previous_frame_.zram_stat[0];
    current_sample.swap_sectors_write =
        current_frame.zram_stat[1] - previous_frame_.zram_stat[1];
    current_sample.swap_waiting_time_ms =
        current_frame.zram_stat[2] - previous_frame_.zram_stat[2];
  }
  current_sample.cpu_temperature = current_frame.cpu_temperature_;
  DCHECK_GE(current_sample.swap_sectors_read, 0);
  DCHECK_GE(current_sample.swap_sectors_write, 0);
  DCHECK_GE(current_sample.swap_waiting_time_ms, 0);
  DCHECK_GE(current_sample.mem_total_kb, 0);
  DCHECK_GE(current_sample.mem_used_kb, 0);
  previous_frame_ = current_frame;
  ++write_index_;
}

bool ParseStatFile(const base::FilePath& path,
                   const int* columns,
                   int64_t* output) {
  char buffer[128];
  const int read = base::ReadFile(path, buffer, sizeof(buffer) - 1);
  if (read < 0)
    return false;
  buffer[read] = 0;
  int column_index = 0;
  const char* scan = buffer;
  while (true) {
    // Skip whitespace.
    while (IsWhitespace(*scan))
      ++scan;
    if (*columns != column_index) {
      // Just skip this entry. It may be digits or text.
      while (!IsWhitespace(*scan))
        ++scan;
    } else {
      int64_t value = 0;
      while (IsDigit(*scan)) {
        value = 10 * value + *scan - '0';
        ++scan;
      }
      *output++ = value;
      ++columns;
      if (*columns < 0)
        return IsEnd(*scan);  // All columns are read.
    }
    if (!IsWhitespace(*scan))
      return false;
    ++column_index;
  }
}

}  // namespace arc
