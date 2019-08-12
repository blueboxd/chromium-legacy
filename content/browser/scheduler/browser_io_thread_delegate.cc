// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_io_thread_delegate.h"

#include "base/message_loop/message_pump_type.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue.h"
#include "content/public/browser/browser_thread.h"

namespace content {

using ::base::sequence_manager::CreateUnboundSequenceManager;
using ::base::sequence_manager::SequenceManager;
using ::base::sequence_manager::TaskQueue;

BrowserIOThreadDelegate::BrowserIOThreadDelegate()
    : sequence_manager_(CreateUnboundSequenceManager(
          SequenceManager::Settings::Builder()
              .SetMessagePumpType(base::MessagePumpType::IO)
              .Build())) {
  Init(sequence_manager_.get());
}

BrowserIOThreadDelegate::BrowserIOThreadDelegate(
    SequenceManager* sequence_manager)
    : sequence_manager_(nullptr) {
  Init(sequence_manager);
}

void BrowserIOThreadDelegate::Init(
    base::sequence_manager::SequenceManager* sequence_manager) {
  task_queues_ = std::make_unique<BrowserTaskQueues>(
      BrowserThread::IO, sequence_manager,
      sequence_manager->GetRealTimeDomain());
  default_task_runner_ = task_queues_->GetHandle()->GetDefaultTaskRunner();
}

scoped_refptr<base::SingleThreadTaskRunner>
BrowserIOThreadDelegate::GetDefaultTaskRunner() {
  return default_task_runner_;
}

BrowserIOThreadDelegate::~BrowserIOThreadDelegate() = default;

void BrowserIOThreadDelegate::BindToCurrentThread(
    base::TimerSlack timer_slack) {
  DCHECK(sequence_manager_);
  sequence_manager_->BindToMessagePump(
      base::MessagePump::Create(base::MessagePumpType::IO));
  sequence_manager_->SetTimerSlack(timer_slack);
  sequence_manager_->SetDefaultTaskRunner(GetDefaultTaskRunner());
}

}  // namespace content
