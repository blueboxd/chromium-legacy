// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/parkable_string_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/instrumentation/memory_pressure_listener.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

// Disabling this will cause parkable strings to never be compressed.
// This is useful for headless mode + virtual time. Since virtual time advances
// quickly, strings may be parked too eagerly in that mode.
const base::Feature kCompressParkableStrings{"CompressParkableStrings",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

struct ParkableStringManager::Statistics {
  size_t original_size;
  size_t uncompressed_size;
  size_t compressed_original_size;
  size_t compressed_size;
  size_t metadata_size;
  size_t overhead_size;
  size_t total_size;
  int64_t savings_size;
};

namespace {

bool CompressionEnabled() {
  return base::FeatureList::IsEnabled(kCompressParkableStrings);
}

class OnPurgeMemoryListener : public GarbageCollected<OnPurgeMemoryListener>,
                              public MemoryPressureListener {
  USING_GARBAGE_COLLECTED_MIXIN(OnPurgeMemoryListener);

  void OnPurgeMemory() override {
    if (!CompressionEnabled()) {
      return;
    }
    ParkableStringManager::Instance().PurgeMemory();
  }
};

}  // namespace

const char* ParkableStringManager::kAllocatorDumpName = "parkable_strings";

// Compares not the pointers, but the arrays. Uses pointers to save space.
struct ParkableStringManager::SecureDigestHash {
  STATIC_ONLY(SecureDigestHash);

  static unsigned GetHash(
      const ParkableStringImpl::SecureDigest* const digest) {
    // The first bytes of the hash are as good as anything else.
    return *reinterpret_cast<const unsigned*>(digest->data());
  }

  static inline bool Equal(const ParkableStringImpl::SecureDigest* const a,
                           const ParkableStringImpl::SecureDigest* const b) {
    return a == b ||
           std::equal(a->data(), a->data() + ParkableStringImpl::kDigestSize,
                      b->data());
  }

  static constexpr bool safe_to_compare_to_empty_or_deleted = false;
};

// static
ParkableStringManagerDumpProvider*
ParkableStringManagerDumpProvider::Instance() {
  static ParkableStringManagerDumpProvider instance;
  return &instance;
}

bool ParkableStringManagerDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  return ParkableStringManager::Instance().OnMemoryDump(pmd);
}

ParkableStringManagerDumpProvider::~ParkableStringManagerDumpProvider() =
    default;
ParkableStringManagerDumpProvider::ParkableStringManagerDumpProvider() =
    default;

ParkableStringManager& ParkableStringManager::Instance() {
  DCHECK(IsMainThread());
  DEFINE_STATIC_LOCAL(ParkableStringManager, instance, ());
  return instance;
}

ParkableStringManager::~ParkableStringManager() = default;

void ParkableStringManager::SetRendererBackgrounded(bool backgrounded) {
  DCHECK(IsMainThread());
  backgrounded_ = backgrounded;
}

bool ParkableStringManager::IsRendererBackgrounded() const {
  DCHECK(IsMainThread());
  return backgrounded_;
}

bool ParkableStringManager::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK(IsMainThread());
  base::trace_event::MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump(kAllocatorDumpName);

  Statistics stats = ComputeStatistics();

  dump->AddScalar("size", "bytes", stats.total_size);
  dump->AddScalar("original_size", "bytes", stats.original_size);
  dump->AddScalar("uncompressed_size", "bytes", stats.uncompressed_size);
  dump->AddScalar("compressed_size", "bytes", stats.compressed_size);
  dump->AddScalar("metadata_size", "bytes", stats.metadata_size);
  dump->AddScalar("overhead_size", "bytes", stats.overhead_size);
  // Has to be uint64_t.
  dump->AddScalar("savings_size", "bytes",
                  stats.savings_size > 0 ? stats.savings_size : 0);

  pmd->AddSuballocation(dump->guid(),
                        WTF::Partitions::kAllocatedObjectPoolName);
  return true;
}

// static
bool ParkableStringManager::ShouldPark(const StringImpl& string) {
  // Don't attempt to park strings smaller than this size.
  static constexpr unsigned int kSizeThreshold = 10000;
  // TODO(lizeb): Consider parking non-main thread strings.
  return string.length() > kSizeThreshold && IsMainThread() &&
         CompressionEnabled();
}

scoped_refptr<ParkableStringImpl> ParkableStringManager::Add(
    scoped_refptr<StringImpl>&& string) {
  DCHECK(IsMainThread());

  ScheduleAgingTaskIfNeeded();

  auto string_impl = string;
  auto digest = ParkableStringImpl::HashString(string_impl.get());
  DCHECK(digest.get());

  auto it = unparked_strings_.find(digest.get());
  if (it != unparked_strings_.end())
    return it->value;

  it = parked_strings_.find(digest.get());
  if (it != parked_strings_.end())
    return it->value;

  auto new_parkable = ParkableStringImpl::MakeParkable(std::move(string_impl),
                                                       std::move(digest));

  // No hit, new unparked string.
  auto insert_result =
      unparked_strings_.insert(new_parkable->digest(), new_parkable.get());
  DCHECK(insert_result.is_new_entry);

  // Lazy registration because registering too early can cause crashes on Linux,
  // see crbug.com/930117, and registering without any strings is pointless
  // anyway.
  if (!did_register_memory_pressure_listener_) {
    // No need to ever unregister, as the only ParkableStringManager instance
    // lives forever.
    MemoryPressureListenerRegistry::Instance().RegisterClient(
        MakeGarbageCollected<OnPurgeMemoryListener>());
    did_register_memory_pressure_listener_ = true;
  }

  if (!has_posted_unparking_time_accounting_task_) {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        Thread::Current()->GetTaskRunner();
    DCHECK(task_runner);
    task_runner->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ParkableStringManager::RecordStatisticsAfter5Minutes,
                       base::Unretained(this)),
        base::TimeDelta::FromMinutes(5));
    has_posted_unparking_time_accounting_task_ = true;
  }

  return new_parkable;
}

void ParkableStringManager::Remove(ParkableStringImpl* string) {
  DCHECK(IsMainThread());
  DCHECK(string->may_be_parked());
  DCHECK(string->digest());

  if (string->is_parked()) {
    auto it = parked_strings_.find(string->digest());
    DCHECK(it != parked_strings_.end());
    parked_strings_.erase(it);
  } else {
    auto it = unparked_strings_.find(string->digest());
    DCHECK(it != unparked_strings_.end());
    unparked_strings_.erase(it);
  }
}

void ParkableStringManager::OnParked(ParkableStringImpl* newly_parked_string) {
  DCHECK(IsMainThread());
  DCHECK(newly_parked_string->may_be_parked());
  DCHECK(newly_parked_string->is_parked());
  auto it = unparked_strings_.find(newly_parked_string->digest());
  DCHECK(it != unparked_strings_.end());
  DCHECK_EQ(it->value, newly_parked_string);
  unparked_strings_.erase(it);
  parked_strings_.insert(newly_parked_string->digest(), newly_parked_string);
}

void ParkableStringManager::OnUnparked(ParkableStringImpl* was_parked_string) {
  DCHECK(IsMainThread());
  DCHECK(was_parked_string->may_be_parked());
  DCHECK(!was_parked_string->is_parked());
  auto it = parked_strings_.find(was_parked_string->digest());
  DCHECK(it != parked_strings_.end());
  DCHECK_EQ(it->value, was_parked_string);
  parked_strings_.erase(it);
  unparked_strings_.insert(was_parked_string->digest(), was_parked_string);
  ScheduleAgingTaskIfNeeded();
}

void ParkableStringManager::RecordUnparkingTime(
    base::TimeDelta unparking_time) {
  total_unparking_time_ += unparking_time;
}

void ParkableStringManager::ParkAll(ParkableStringImpl::ParkingMode mode) {
  DCHECK(IsMainThread());
  DCHECK(CompressionEnabled());

  size_t total_size = 0;
  for (const auto& kv : parked_strings_)
    total_size += kv.value->CharactersSizeInBytes();

  // Parking may be synchronous, need to copy values first.
  // In case of synchronous parking, |ParkableStringImpl::Park()| calls
  // |OnParked()|, which moves the string from |unparked_strings_|
  // to |parked_strings_|, hence the need to copy values first.
  //
  // Efficiency: In practice, either we are parking strings for the first time,
  // and |unparked_strings_| can contain a few 10s of strings (and we will
  // trigger expensive compression), or this is a subsequent one, and
  // |unparked_strings_| will have few entries.
  WTF::Vector<ParkableStringImpl*> unparked = GetUnparkedStrings();

  for (ParkableStringImpl* str : unparked) {
    str->Park(mode);
    total_size += str->CharactersSizeInBytes();
  }
}

size_t ParkableStringManager::Size() const {
  return parked_strings_.size() + unparked_strings_.size();
}

void ParkableStringManager::RecordStatisticsAfter5Minutes() const {
  base::UmaHistogramTimes("Memory.ParkableString.MainThreadTime.5min",
                          total_unparking_time_);
  if (base::ThreadTicks::IsSupported()) {
    base::UmaHistogramTimes("Memory.ParkableString.ParkingThreadTime.5min",
                            total_parking_thread_time_);
  }
  Statistics stats = ComputeStatistics();
  base::UmaHistogramCounts100000("Memory.ParkableString.TotalSizeKb.5min",
                                 stats.original_size / 1000);
  base::UmaHistogramCounts100000("Memory.ParkableString.CompressedSizeKb.5min",
                                 stats.compressed_size / 1000);
  size_t savings = stats.compressed_original_size - stats.compressed_size;
  base::UmaHistogramCounts100000("Memory.ParkableString.SavingsKb.5min",
                                 savings / 1000);

  if (stats.compressed_original_size != 0) {
    size_t ratio_percentage =
        (100 * stats.compressed_size) / stats.compressed_original_size;
    base::UmaHistogramPercentage("Memory.ParkableString.CompressionRatio.5min",
                                 ratio_percentage);
  }
}

void ParkableStringManager::AgeStringsAndPark() {
  DCHECK(CompressionEnabled());

  TRACE_EVENT0("blink", "ParkableStringManager::AgeStringsAndPark");
  has_pending_aging_task_ = false;

  WTF::Vector<ParkableStringImpl*> unparked = GetUnparkedStrings();
  bool can_make_progress = false;
  for (ParkableStringImpl* str : unparked) {
    if (str->MaybeAgeOrParkString() ==
        ParkableStringImpl::AgeOrParkResult::kSuccessOrTransientFailure)
      can_make_progress = true;
  }

  // Some strings will never be parkable because there are lasting external
  // references to them. Don't endlessely reschedule the aging task if we are
  // not making progress (that is, no new string was either aged or parked).
  //
  // This ensures that the tasks will stop getting scheduled, assuming that
  // the renderer is otherwise idle. Note that we cannot use "idle" tasks as
  // we need to age and park strings after the renderer becomes idle, meaning
  // that this has to run when the idle tasks are not. As a consequence, it
  // is important to make sure that this will not reschedule tasks forever.
  bool reschedule = !unparked_strings_.IsEmpty() && can_make_progress;
  if (reschedule)
    ScheduleAgingTaskIfNeeded();
}

void ParkableStringManager::ScheduleAgingTaskIfNeeded() {
  if (!CompressionEnabled())
    return;

  if (has_pending_aging_task_)
    return;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      Thread::Current()->GetTaskRunner();
  task_runner->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ParkableStringManager::AgeStringsAndPark,
                     base::Unretained(this)),
      base::TimeDelta::FromSeconds(kAgingIntervalInSeconds));
  has_pending_aging_task_ = true;
}

void ParkableStringManager::PurgeMemory() {
  DCHECK(IsMainThread());
  DCHECK(CompressionEnabled());

  ParkAll(ParkableStringImpl::ParkingMode::kAlways);
  // Critical memory pressure: drop compressed data for strings that we cannot
  // park now.
  //
  // After |ParkAll()| has been called, parkable strings have either been parked
  // synchronously (and no longer in |unparked_strings_|), or being parked and
  // purging is a no-op.
  if (!IsRendererBackgrounded()) {
    for (const auto& kv : unparked_strings_)
      kv.value->PurgeMemory();
  }
}

Vector<ParkableStringImpl*> ParkableStringManager::GetUnparkedStrings() const {
  WTF::Vector<ParkableStringImpl*> unparked;
  unparked.ReserveCapacity(unparked_strings_.size());
  for (const auto& kv : unparked_strings_)
    unparked.push_back(kv.value);

  return unparked;
}

ParkableStringManager::Statistics ParkableStringManager::ComputeStatistics()
    const {
  ParkableStringManager::Statistics stats = {};
  // The digest has an inline capacity set to the digest size, hence sizeof() is
  // accurate.
  constexpr size_t kParkableStringImplActualSize =
      sizeof(ParkableStringImpl) + sizeof(ParkableStringImpl::ParkableMetadata);

  for (const auto& kv : unparked_strings_) {
    ParkableStringImpl* str = kv.value;
    size_t size = str->CharactersSizeInBytes();
    stats.original_size += size;
    stats.uncompressed_size += size;
    stats.metadata_size += kParkableStringImplActualSize;

    if (str->has_compressed_data())
      stats.overhead_size += str->compressed_size();

    // Since ParkableStringManager wants to have a finer breakdown of memory
    // footprint, this doesn't directly use
    // |ParkableStringImpl::MemoryFootprintForDump()|. However we want the two
    // computations to be consistent, hence the DCHECK().
    size_t memory_footprint =
        (str->has_compressed_data() ? str->compressed_size() : 0) + size +
        kParkableStringImplActualSize;
    DCHECK_EQ(memory_footprint, str->MemoryFootprintForDump());
  }

  for (const auto& kv : parked_strings_) {
    ParkableStringImpl* str = kv.value;
    size_t size = str->CharactersSizeInBytes();
    stats.compressed_original_size += size;
    stats.original_size += size;
    stats.compressed_size += str->compressed_size();
    stats.metadata_size += kParkableStringImplActualSize;

    // See comment above.
    size_t memory_footprint =
        str->compressed_size() + kParkableStringImplActualSize;
    DCHECK_EQ(memory_footprint, str->MemoryFootprintForDump());
  }

  stats.total_size = stats.uncompressed_size + stats.compressed_size +
                     stats.metadata_size + stats.overhead_size;
  size_t memory_footprint = stats.compressed_size + stats.uncompressed_size +
                            stats.metadata_size + stats.overhead_size;
  stats.savings_size =
      stats.original_size - static_cast<int64_t>(memory_footprint);

  return stats;
}

void ParkableStringManager::ResetForTesting() {
  backgrounded_ = false;
  has_pending_aging_task_ = false;
  has_posted_unparking_time_accounting_task_ = false;
  did_register_memory_pressure_listener_ = false;
  total_unparking_time_ = base::TimeDelta();
  total_parking_thread_time_ = base::TimeDelta();
  unparked_strings_.clear();
  parked_strings_.clear();
}

ParkableStringManager::ParkableStringManager()
    : backgrounded_(false),
      has_pending_aging_task_(false),
      has_posted_unparking_time_accounting_task_(false),
      did_register_memory_pressure_listener_(false),
      total_unparking_time_(),
      total_parking_thread_time_(),
      unparked_strings_(),
      parked_strings_() {}

}  // namespace blink
