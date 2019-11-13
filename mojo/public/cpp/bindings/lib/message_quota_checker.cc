// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/message_quota_checker.h"

#include <algorithm>

#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/synchronization/lock.h"
#include "mojo/public/c/system/quota.h"
#include "mojo/public/cpp/bindings/features.h"
#include "mojo/public/cpp/bindings/mojo_buildflags.h"

namespace mojo {
namespace internal {
namespace {

const base::FeatureParam<int> kMojoRecordUnreadMessageCountSampleRate = {
    &features::kMojoRecordUnreadMessageCount, "SampleRate",
    100  // Sample 1% of Connectors by default. */
};

const base::FeatureParam<int> kMojoRecordUnreadMessageCountQuotaValue = {
    &features::kMojoRecordUnreadMessageCount, "QuotaValue",
    100  // Use a 100 message quote by default.
};

const base::FeatureParam<int> kMojoRecordUnreadMessageCountCrashThreshold = {
    &features::kMojoRecordUnreadMessageCount, "CrashThreshold",
    0  // Set to zero to disable crash dumps by default.
};

NOINLINE void MaybeDumpWithoutCrashing(size_t quota_used) {
  static bool have_crashed = false;
  if (have_crashed)
    return;

  // Only crash once per process/per run. Note that this is slightly racy
  // against concurrent quota overruns on multiple threads, but that's fine.
  have_crashed = true;

  // This is happening because the user of the interface implicated on the crash
  // stack has queued up an unreasonable number of messages, namely
  // |quota_used|.
  base::debug::DumpWithoutCrashing();

  base::debug::Alias(&quota_used);
}

}  // namespace

// static
scoped_refptr<MessageQuotaChecker> MessageQuotaChecker::MaybeCreate() {
  static const Configuration config = GetConfiguration();
  return MaybeCreateImpl(config);
}

void MessageQuotaChecker::BeforeWrite() {
  QuotaCheckImpl(0u);
}

void MessageQuotaChecker::BeforeMessagesEnqueued(size_t num) {
  DCHECK_NE(num, 0u);
  QuotaCheckImpl(num);
}

void MessageQuotaChecker::AfterMessagesDequeued(size_t num) {
  base::AutoLock hold(lock_);
  DCHECK_LE(num, consumed_quota_);
  DCHECK_NE(num, 0u);

  consumed_quota_ -= num;
}

size_t MessageQuotaChecker::GetMaxQuotaUsage() {
  base::AutoLock hold(lock_);
  return max_consumed_quota_;
}

void MessageQuotaChecker::SetMessagePipe(MessagePipeHandle message_pipe) {
  base::AutoLock hold(lock_);
  message_pipe_ = message_pipe;
  if (!message_pipe_)
    return;

  MojoResult rv =
      MojoSetQuota(message_pipe.value(), MOJO_QUOTA_TYPE_UNREAD_MESSAGE_COUNT,
                   config_->unread_message_count_quota, nullptr);
  DCHECK_EQ(MOJO_RESULT_OK, rv);
}

size_t MessageQuotaChecker::GetCurrentQuotaStatusForTesting() {
  base::AutoLock hold(lock_);
  return GetCurrentQuotaStatus();
}

// static
MessageQuotaChecker::Configuration
MessageQuotaChecker::GetConfigurationForTesting() {
  return GetConfiguration();
}

// static
scoped_refptr<MessageQuotaChecker> MessageQuotaChecker::MaybeCreateForTesting(
    const Configuration& config) {
  return MaybeCreateImpl(config);
}

MessageQuotaChecker::MessageQuotaChecker(const Configuration* config)
    : config_(config) {}
MessageQuotaChecker::~MessageQuotaChecker() = default;

// static
MessageQuotaChecker::Configuration MessageQuotaChecker::GetConfiguration() {
  Configuration ret;

  ret.is_enabled =
      base::FeatureList::IsEnabled(features::kMojoRecordUnreadMessageCount);
  ret.sample_rate = kMojoRecordUnreadMessageCountSampleRate.Get();

  // Lower-bound the quota value to 100, which implies roughly 2% message
  // overhead for sampled pipes.
  constexpr int kMinQuotaValue = 100;
  ret.unread_message_count_quota =
      std::max(kMinQuotaValue, kMojoRecordUnreadMessageCountQuotaValue.Get());
  ret.crash_threshold = kMojoRecordUnreadMessageCountCrashThreshold.Get();
  ret.maybe_crash_function = &MaybeDumpWithoutCrashing;
  return ret;
}

// static
scoped_refptr<MessageQuotaChecker> MessageQuotaChecker::MaybeCreateImpl(
    const Configuration& config) {
  if (!config.is_enabled)
    return nullptr;

  if (base::RandInt(0, config.sample_rate - 1) != 0)
    return nullptr;

  return new MessageQuotaChecker(&config);
}

size_t MessageQuotaChecker::GetCurrentQuotaStatus() {
  lock_.AssertAcquired();

  size_t quota_status = consumed_quota_;
  if (message_pipe_) {
    uint64_t limit = 0;
    uint64_t usage = 0;
    MojoResult rv = MojoQueryQuota(message_pipe_.value(),
                                   MOJO_QUOTA_TYPE_UNREAD_MESSAGE_COUNT,
                                   nullptr, &limit, &usage);
    if (rv == MOJO_RESULT_OK)
      quota_status += usage;
  }

  return quota_status;
}

void MessageQuotaChecker::QuotaCheckImpl(size_t num_enqueued) {
  bool new_max = false;
  size_t quota_used = 0u;
  {
    base::AutoLock hold(lock_);

    consumed_quota_ += num_enqueued;
    quota_used = GetCurrentQuotaStatus();

    // Account for the message that will be written.
    if (!num_enqueued)
      ++quota_used;

    if (quota_used > max_consumed_quota_) {
      max_consumed_quota_ = quota_used;
      new_max = true;
    }
  }

  if (new_max && config_->crash_threshold != 0 &&
      quota_used >= config_->crash_threshold) {
    config_->maybe_crash_function(quota_used);
  }
}

}  // namespace internal
}  // namespace mojo
