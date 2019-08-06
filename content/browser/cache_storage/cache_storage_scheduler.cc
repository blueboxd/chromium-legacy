// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_scheduler.h"

#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "content/browser/cache_storage/cache_storage_histogram_utils.h"
#include "content/browser/cache_storage/cache_storage_operation.h"

namespace content {

CacheStorageScheduler::CacheStorageScheduler(
    CacheStorageSchedulerClient client_type,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)), client_type_(client_type) {}

CacheStorageScheduler::~CacheStorageScheduler() {}

void CacheStorageScheduler::ScheduleOperation(CacheStorageSchedulerOp op_type,
                                              base::OnceClosure closure) {
  RecordCacheStorageSchedulerUMA(CacheStorageSchedulerUMA::kQueueLength,
                                 client_type_, op_type,
                                 pending_operations_.size());

  pending_operations_.push_back(std::make_unique<CacheStorageOperation>(
      std::move(closure), client_type_, op_type, task_runner_));
  RunOperationIfIdle();
}

void CacheStorageScheduler::CompleteOperationAndRunNext() {
  DCHECK(running_operation_);
  running_operation_.reset();

  RunOperationIfIdle();
}

bool CacheStorageScheduler::ScheduledOperations() const {
  return running_operation_ || !pending_operations_.empty();
}

void CacheStorageScheduler::DispatchOperationTask(base::OnceClosure task) {
  task_runner_->PostTask(FROM_HERE, std::move(task));
}

void CacheStorageScheduler::RunOperationIfIdle() {
  if (!running_operation_ && !pending_operations_.empty()) {
    // TODO(jkarlin): Run multiple operations in parallel where allowed.
    running_operation_ = std::move(pending_operations_.front());
    pending_operations_.pop_front();

    RecordCacheStorageSchedulerUMA(
        CacheStorageSchedulerUMA::kQueueDuration, client_type_,
        running_operation_->op_type(),
        base::TimeTicks::Now() - running_operation_->creation_ticks());

    DispatchOperationTask(base::BindOnce(&CacheStorageOperation::Run,
                                         running_operation_->AsWeakPtr()));
  }
}

}  // namespace content
