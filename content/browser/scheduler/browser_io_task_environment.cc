// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_io_task_environment.h"

#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue.h"
#include "content/public/browser/browser_thread.h"

namespace content {

using ::base::sequence_manager::CreateUnboundSequenceManager;
using ::base::sequence_manager::SequenceManager;
using ::base::sequence_manager::TaskQueue;

BrowserIOTaskEnvironment::BrowserIOTaskEnvironment()
    : sequence_manager_(CreateUnboundSequenceManager(
          SequenceManager::Settings::Builder()
              .SetMessagePumpType(base::MessagePump::Type::IO)
              .Build())) {
  Init(sequence_manager_.get());
}

BrowserIOTaskEnvironment::BrowserIOTaskEnvironment(
    SequenceManager* sequence_manager)
    : sequence_manager_(nullptr) {
  Init(sequence_manager);
}

void BrowserIOTaskEnvironment::Init(
    base::sequence_manager::SequenceManager* sequence_manager) {
  task_queues_ = std::make_unique<BrowserTaskQueues>(
      BrowserThread::IO, sequence_manager,
      sequence_manager->GetRealTimeDomain());
  default_task_runner_ = task_queues_->GetHandle()->GetDefaultTaskRunner();
}

scoped_refptr<base::SingleThreadTaskRunner>
BrowserIOTaskEnvironment::GetDefaultTaskRunner() {
  return default_task_runner_;
}

BrowserIOTaskEnvironment::~BrowserIOTaskEnvironment() = default;

void BrowserIOTaskEnvironment::BindToCurrentThread(
    base::TimerSlack timer_slack) {
  DCHECK(sequence_manager_);
  sequence_manager_->BindToMessagePump(
      base::MessagePump::Create(base::MessagePump::Type::IO));
  sequence_manager_->SetTimerSlack(timer_slack);
  sequence_manager_->SetDefaultTaskRunner(GetDefaultTaskRunner());
}

}  // namespace content
