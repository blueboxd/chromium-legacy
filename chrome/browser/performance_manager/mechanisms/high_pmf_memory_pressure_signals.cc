// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/high_pmf_memory_pressure_signals.h"
#include <memory>
#include "base/bind.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/task/post_task.h"
#include "base/util/memory_pressure/memory_pressure_voter.h"
#include "base/util/memory_pressure/multi_source_memory_pressure_monitor.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace performance_manager {
namespace mechanism {

// Wrapper around a MemoryPressureVoter living on the UI thread.
class MemoryPressureVoterOnUIThread {
 public:
  MemoryPressureVoterOnUIThread() = default;
  virtual ~MemoryPressureVoterOnUIThread() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }
  MemoryPressureVoterOnUIThread(const MemoryPressureVoterOnUIThread& other) =
      delete;
  MemoryPressureVoterOnUIThread& operator=(
      const MemoryPressureVoterOnUIThread&) = delete;

  void InitOnUIThread() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    voter_ = static_cast<util::MultiSourceMemoryPressureMonitor*>(
                 base::MemoryPressureMonitor::Get())
                 ->CreateVoter();
  }

  void SetVote(base::MemoryPressureListener::MemoryPressureLevel level,
               bool notify) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    voter_->SetVote(level, notify);
  }

 private:
  std::unique_ptr<util::MemoryPressureVoter> voter_;
};

HighPMFMemoryPressureSignals::HighPMFMemoryPressureSignals() {
  ui_thread_voter_ = std::make_unique<MemoryPressureVoterOnUIThread>();
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&MemoryPressureVoterOnUIThread::InitOnUIThread,
                                base::Unretained(ui_thread_voter_.get())));
}

HighPMFMemoryPressureSignals::~HighPMFMemoryPressureSignals() {
  base::DeleteSoon(FROM_HERE, {content::BrowserThread::UI},
                   std::move(ui_thread_voter_));
}

void HighPMFMemoryPressureSignals::SetPressureLevel(MemoryPressureLevel level) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool notify = level == MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_CRITICAL;
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&MemoryPressureVoterOnUIThread::SetVote,
                     base::Unretained(ui_thread_voter_.get()), level, notify));
  pressure_level_ = level;
}

}  // namespace mechanism
}  // namespace performance_manager
