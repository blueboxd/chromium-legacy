// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_task_executor.h"

#include <atomic>

#include "base/bind.h"
#include "base/deferred_sequenced_task_runner.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/browser/browser_process_sub_thread.h"
#include "content/browser/browser_thread_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/common/content_features.h"

#if defined(OS_ANDROID)
#include "base/android/task_scheduler/post_task_android.h"
#endif

namespace content {
namespace {

using QueueType = ::content::BrowserTaskQueues::QueueType;

// |g_browser_task_executor| is intentionally leaked on shutdown.
BrowserTaskExecutor* g_browser_task_executor = nullptr;

QueueType GetQueueType(const base::TaskTraits& traits,
                       BrowserTaskType task_type) {
  switch (task_type) {
    case BrowserTaskType::kBootstrap:
      // Note we currently ignore the priority for bootstrap tasks.
      return QueueType::kBootstrap;

    case BrowserTaskType::kNavigation:
    case BrowserTaskType::kPreconnect:
      // Note we currently ignore the priority for navigation and preconnection
      // tasks.
      return QueueType::kNavigationAndPreconnection;

    case BrowserTaskType::kDefault:
      // Defer to traits.priority() below.
      break;

    case BrowserTaskType::kBrowserTaskType_Last:
      NOTREACHED();
  }

  switch (traits.priority()) {
    case base::TaskPriority::BEST_EFFORT:
      return QueueType::kBestEffort;

    case base::TaskPriority::USER_VISIBLE:
      return QueueType::kUserVisible;

    case base::TaskPriority::USER_BLOCKING:
      return QueueType::kUserBlocking;
  }
}

}  // namespace

BrowserTaskExecutor::BrowserTaskExecutor(
    std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler,
    std::unique_ptr<BrowserIOTaskEnvironment> browser_io_task_environment)
    : browser_ui_thread_scheduler_(std::move(browser_ui_thread_scheduler)),
      browser_ui_thread_handle_(browser_ui_thread_scheduler_->GetHandle()),
      browser_io_task_environment_(std::move(browser_io_task_environment)),
      browser_io_thread_handle_(browser_io_task_environment_->CreateHandle()) {}

BrowserTaskExecutor::~BrowserTaskExecutor() = default;

// static
void BrowserTaskExecutor::Create() {
  DCHECK(!base::ThreadTaskRunnerHandle::IsSet());
  CreateInternal(std::make_unique<BrowserUIThreadScheduler>(),
                 std::make_unique<BrowserIOTaskEnvironment>());
}

// static
void BrowserTaskExecutor::CreateForTesting(
    std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler,
    std::unique_ptr<BrowserIOTaskEnvironment> browser_io_task_environment) {
  CreateInternal(std::move(browser_ui_thread_scheduler),
                 std::move(browser_io_task_environment));
}

// static
void BrowserTaskExecutor::CreateInternal(
    std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler,
    std::unique_ptr<BrowserIOTaskEnvironment> browser_io_task_environment) {
  DCHECK(!g_browser_task_executor);
  g_browser_task_executor =
      new BrowserTaskExecutor(std::move(browser_ui_thread_scheduler),
                              std::move(browser_io_task_environment));
  base::RegisterTaskExecutor(BrowserTaskTraitsExtension::kExtensionId,
                             g_browser_task_executor);
  g_browser_task_executor->browser_ui_thread_handle_
      ->EnableAllExceptBestEffortQueues();

#if defined(OS_ANDROID)
  base::PostTaskAndroid::SignalNativeSchedulerReady();
#endif
}

// static
void BrowserTaskExecutor::ResetForTesting() {
#if defined(OS_ANDROID)
  base::PostTaskAndroid::SignalNativeSchedulerShutdown();
#endif

  if (g_browser_task_executor) {
    base::UnregisterTaskExecutorForTesting(
        BrowserTaskTraitsExtension::kExtensionId);
    delete g_browser_task_executor;
    g_browser_task_executor = nullptr;
  }
}

// static
void BrowserTaskExecutor::PostFeatureListSetup() {
  DCHECK(g_browser_task_executor);
  DCHECK(g_browser_task_executor->browser_ui_thread_scheduler_);
  DCHECK(g_browser_task_executor->browser_io_task_environment_);
  g_browser_task_executor->browser_ui_thread_handle_
      ->PostFeatureListInitializationSetup();
  g_browser_task_executor->browser_io_thread_handle_
      ->PostFeatureListInitializationSetup();
}

// static
void BrowserTaskExecutor::Shutdown() {
  if (!g_browser_task_executor)
    return;

  DCHECK(g_browser_task_executor->browser_ui_thread_scheduler_);
  // We don't delete |g_browser_task_executor| because other threads may
  // PostTask or call BrowserTaskExecutor::GetTaskRunner while we're tearing
  // things down. We don't want to add locks so we just leak instead of dealing
  // with that. For similar reasons we don't need to call
  // PostTaskAndroid::SignalNativeSchedulerShutdown on Android. In tests however
  // we need to clean up, so BrowserTaskExecutor::ResetForTesting should be
  // called.
  g_browser_task_executor->browser_ui_thread_scheduler_.reset();
  g_browser_task_executor->browser_io_task_environment_.reset();
}

// static
void BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(
    BrowserThread::ID identifier) {
  DCHECK(g_browser_task_executor);

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);

  switch (identifier) {
    case BrowserThread::UI:
      g_browser_task_executor->browser_ui_thread_handle_
          ->ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
      break;
    case BrowserThread::IO: {
      g_browser_task_executor->browser_io_thread_handle_
          ->ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
      break;
    }
    case BrowserThread::ID_COUNT:
      NOTREACHED();
  }

  run_loop.Run();
}

bool BrowserTaskExecutor::PostDelayedTask(const base::Location& from_here,
                                          const base::TaskTraits& traits,
                                          base::OnceClosure task,
                                          base::TimeDelta delay) {
  DCHECK_EQ(BrowserTaskTraitsExtension::kExtensionId, traits.extension_id());
  const BrowserTaskTraitsExtension& extension =
      traits.GetExtension<BrowserTaskTraitsExtension>();
  if (extension.nestable()) {
    return GetTaskRunner(traits)->PostDelayedTask(from_here, std::move(task),
                                                  delay);
  } else {
    return GetTaskRunner(traits)->PostNonNestableDelayedTask(
        from_here, std::move(task), delay);
  }
}

scoped_refptr<base::TaskRunner> BrowserTaskExecutor::CreateTaskRunner(
    const base::TaskTraits& traits) {
  return GetTaskRunner(traits);
}

scoped_refptr<base::SequencedTaskRunner>
BrowserTaskExecutor::CreateSequencedTaskRunner(const base::TaskTraits& traits) {
  return GetTaskRunner(traits);
}

scoped_refptr<base::SingleThreadTaskRunner>
BrowserTaskExecutor::CreateSingleThreadTaskRunner(
    const base::TaskTraits& traits,
    base::SingleThreadTaskRunnerThreadMode thread_mode) {
  return GetTaskRunner(traits);
}

#if defined(OS_WIN)
scoped_refptr<base::SingleThreadTaskRunner>
BrowserTaskExecutor::CreateCOMSTATaskRunner(
    const base::TaskTraits& traits,
    base::SingleThreadTaskRunnerThreadMode thread_mode) {
  return GetTaskRunner(traits);
}
#endif  // defined(OS_WIN)

scoped_refptr<base::SingleThreadTaskRunner> BrowserTaskExecutor::GetTaskRunner(
    const base::TaskTraits& traits) const {
  auto id_and_queue = GetThreadIdAndQueueType(traits);

  switch (id_and_queue.thread_id) {
    case BrowserThread::UI: {
      return browser_ui_thread_handle_->GetBrowserTaskRunner(
          id_and_queue.queue_type);
    }
    case BrowserThread::IO:
      return browser_io_thread_handle_->GetBrowserTaskRunner(
          id_and_queue.queue_type);
    case BrowserThread::ID_COUNT:
      NOTREACHED();
  }
  return nullptr;
}

// static
BrowserTaskExecutor::ThreadIdAndQueueType
BrowserTaskExecutor::GetThreadIdAndQueueType(const base::TaskTraits& traits) {
  DCHECK_EQ(BrowserTaskTraitsExtension::kExtensionId, traits.extension_id());
  BrowserTaskTraitsExtension extension =
      traits.GetExtension<BrowserTaskTraitsExtension>();

  BrowserThread::ID thread_id = extension.browser_thread();
  DCHECK_GE(thread_id, 0);

  BrowserTaskType task_type = extension.task_type();
  DCHECK_LT(task_type, BrowserTaskType::kBrowserTaskType_Last);

  return {thread_id, GetQueueType(traits, task_type)};
}

// static
void BrowserTaskExecutor::EnableAllQueues() {
  DCHECK(g_browser_task_executor);
  g_browser_task_executor->browser_ui_thread_handle_->EnableAllQueues();
  g_browser_task_executor->browser_io_thread_handle_->EnableAllQueues();
}

// static
void BrowserTaskExecutor::InitializeIOThread() {
  DCHECK(g_browser_task_executor);
  g_browser_task_executor->browser_io_thread_handle_
      ->EnableAllExceptBestEffortQueues();
}

std::unique_ptr<BrowserProcessSubThread> BrowserTaskExecutor::CreateIOThread() {
  DCHECK(g_browser_task_executor);
  DCHECK(g_browser_task_executor->browser_io_task_environment_);
  TRACE_EVENT0("startup", "BrowserTaskExecutor::CreateIOThread");

  auto io_thread = std::make_unique<BrowserProcessSubThread>(BrowserThread::IO);

  if (g_browser_task_executor->browser_io_task_environment_
          ->allow_blocking_for_testing()) {
    io_thread->AllowBlockingForTesting();
  }

  base::Thread::Options options;
  options.message_loop_type = base::MessagePump::Type::IO;
  options.task_environment =
      g_browser_task_executor->browser_io_task_environment_.release();
  // Up the priority of the |io_thread_| as some of its IPCs relate to
  // display tasks.
  if (base::FeatureList::IsEnabled(features::kBrowserUseDisplayThreadPriority))
    options.priority = base::ThreadPriority::DISPLAY;
  if (!io_thread->StartWithOptions(options))
    LOG(FATAL) << "Failed to start BrowserThread:IO";
  return io_thread;
}

#if DCHECK_IS_ON()

// static
void BrowserTaskExecutor::AddValidator(
    const base::TaskTraits& traits,
    BrowserTaskQueues::Validator* validator) {
  if (!g_browser_task_executor)
    return;

  auto id_and_queue = g_browser_task_executor->GetThreadIdAndQueueType(traits);
  switch (id_and_queue.thread_id) {
    case BrowserThread::ID::IO:
      g_browser_task_executor->browser_io_thread_handle_->AddValidator(
          id_and_queue.queue_type, validator);
      break;

    case BrowserThread::ID::UI:
      g_browser_task_executor->browser_ui_thread_handle_->AddValidator(
          id_and_queue.queue_type, validator);
      break;

    case BrowserThread::ID::ID_COUNT:
      NOTREACHED();
      break;
  }
}

// static
void BrowserTaskExecutor::RemoveValidator(
    const base::TaskTraits& traits,
    BrowserTaskQueues::Validator* validator) {
  if (!g_browser_task_executor)
    return;

  auto id_and_queue = g_browser_task_executor->GetThreadIdAndQueueType(traits);
  switch (id_and_queue.thread_id) {
    case BrowserThread::ID::IO:
      g_browser_task_executor->browser_io_thread_handle_->RemoveValidator(
          id_and_queue.queue_type, validator);
      break;

    case BrowserThread::ID::UI:
      g_browser_task_executor->browser_ui_thread_handle_->RemoveValidator(
          id_and_queue.queue_type, validator);
      break;

    case BrowserThread::ID::ID_COUNT:
      NOTREACHED();
      break;
  }
}

#endif

}  // namespace content
