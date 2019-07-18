// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/utility_thread_impl.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/simple_connection_filter.h"
#include "content/public/utility/content_utility_client.h"
#include "content/utility/services.h"
#include "content/utility/utility_blink_platform_with_sandbox_support_impl.h"
#include "content/utility/utility_service_factory.h"
#include "ipc/ipc_sync_channel.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/sandbox/switches.h"

#if !defined(OS_ANDROID)
#include "content/public/common/resource_usage_reporter.mojom.h"
#include "net/proxy_resolution/proxy_resolver_v8.h"
#endif

namespace content {

namespace {

class ServiceBinderImpl {
 public:
  explicit ServiceBinderImpl(
      scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner)
      : main_thread_task_runner_(std::move(main_thread_task_runner)) {}
  ~ServiceBinderImpl() = default;

  void BindServiceInterface(mojo::GenericPendingReceiver receiver) {
    // We watch for and terminate on PEER_CLOSED, but we also terminate if the
    // watcher is cancelled (meaning the local endpoint was closed rather than
    // the peer). Hence any breakage of the service pipe leads to termination.
    auto watcher = std::make_unique<mojo::SimpleWatcher>(
        FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC);
    watcher->Watch(receiver.pipe(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                   MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                   base::BindRepeating(&ServiceBinderImpl::OnServicePipeClosed,
                                       base::Unretained(this), watcher.get()));
    service_pipe_watchers_.insert(std::move(watcher));
    HandleServiceRequestOnIOThread(std::move(receiver),
                                   main_thread_task_runner_.get());
  }

 private:
  void OnServicePipeClosed(mojo::SimpleWatcher* which,
                           MojoResult result,
                           const mojo::HandleSignalsState& state) {
    // NOTE: It doesn't matter whether this was peer closure or local closure,
    // and those are the only two ways this method can be invoked.

    auto it = service_pipe_watchers_.find(which);
    DCHECK(it != service_pipe_watchers_.end());
    service_pipe_watchers_.erase(it);

    // No more services running in this process.
    if (service_pipe_watchers_.empty()) {
      main_thread_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce([] { UtilityThread::Get()->ReleaseProcess(); }));
    }
  }

  const scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner_;

  // These trap signals on any (unowned) primordial service pipes. We don't
  // actually care about the signals so these never get armed. We only watch for
  // cancellation, because that means the service's primordial pipe handle was
  // closed locally and we treat that as the service calling it quits.
  std::set<std::unique_ptr<mojo::SimpleWatcher>, base::UniquePtrComparator>
      service_pipe_watchers_;

  DISALLOW_COPY_AND_ASSIGN(ServiceBinderImpl);
};

ChildThreadImpl::Options::ServiceBinder GetServiceBinder() {
  static base::NoDestructor<ServiceBinderImpl> binder(
      base::ThreadTaskRunnerHandle::Get());
  return base::BindRepeating(&ServiceBinderImpl::BindServiceInterface,
                             base::Unretained(binder.get()));
}

}  // namespace

#if !defined(OS_ANDROID)
class ResourceUsageReporterImpl : public mojom::ResourceUsageReporter {
 public:
  ResourceUsageReporterImpl() {}
  ~ResourceUsageReporterImpl() override {}

 private:
  void GetUsageData(GetUsageDataCallback callback) override {
    mojom::ResourceUsageDataPtr data = mojom::ResourceUsageData::New();
    size_t total_heap_size = net::ProxyResolverV8::GetTotalHeapSize();
    if (total_heap_size) {
      data->reports_v8_stats = true;
      data->v8_bytes_allocated = total_heap_size;
      data->v8_bytes_used = net::ProxyResolverV8::GetUsedHeapSize();
    }
    std::move(callback).Run(std::move(data));
  }

  DISALLOW_COPY_AND_ASSIGN(ResourceUsageReporterImpl);
};

void CreateResourceUsageReporter(mojom::ResourceUsageReporterRequest request) {
  mojo::MakeStrongBinding(std::make_unique<ResourceUsageReporterImpl>(),
                          std::move(request));
}
#endif  // !defined(OS_ANDROID)

UtilityThreadImpl::UtilityThreadImpl(base::RepeatingClosure quit_closure)
    : ChildThreadImpl(std::move(quit_closure),
                      ChildThreadImpl::Options::Builder()
                          .AutoStartServiceManagerConnection(false)
                          .ServiceBinder(GetServiceBinder())
                          .Build()) {
  Init();
}

UtilityThreadImpl::UtilityThreadImpl(const InProcessChildThreadParams& params)
    : ChildThreadImpl(base::DoNothing(),
                      ChildThreadImpl::Options::Builder()
                          .AutoStartServiceManagerConnection(false)
                          .InBrowserProcess(params)
                          .ServiceBinder(GetServiceBinder())
                          .Build()) {
  Init();
}

UtilityThreadImpl::~UtilityThreadImpl() = default;

void UtilityThreadImpl::Shutdown() {
  ChildThreadImpl::Shutdown();
}

void UtilityThreadImpl::ReleaseProcess() {
  if (!IsInBrowserProcess()) {
    ChildProcess::current()->ReleaseProcess();
    return;
  }

  // Close the channel to cause the UtilityProcessHost to be deleted. We need to
  // take a different code path than the multi-process case because that case
  // depends on the child process going away to close the channel, but that
  // can't happen when we're in single process mode.
  channel()->Close();
}

void UtilityThreadImpl::EnsureBlinkInitialized() {
  EnsureBlinkInitializedInternal(/*sandbox_support=*/false);
}

#if defined(OS_POSIX) && !defined(OS_ANDROID)
void UtilityThreadImpl::EnsureBlinkInitializedWithSandboxSupport() {
  EnsureBlinkInitializedInternal(/*sandbox_support=*/true);
}
#endif

void UtilityThreadImpl::EnsureBlinkInitializedInternal(bool sandbox_support) {
  if (blink_platform_impl_)
    return;

  // We can only initialize Blink on one thread, and in single process mode
  // we run the utility thread on a separate thread. This means that if any
  // code needs Blink initialized in the utility process, they need to have
  // another path to support single process mode.
  if (IsInBrowserProcess())
    return;

  blink_platform_impl_ =
      sandbox_support
          ? std::make_unique<UtilityBlinkPlatformWithSandboxSupportImpl>(
                GetConnector())
          : std::make_unique<blink::Platform>();
  blink::Platform::CreateMainThreadAndInitialize(blink_platform_impl_.get());
}

void UtilityThreadImpl::Init() {
  ChildProcess::current()->AddRefProcess();

  auto registry = std::make_unique<service_manager::BinderRegistry>();
#if !defined(OS_ANDROID)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          service_manager::switches::kNoneSandboxAndElevatedPrivileges)) {
    registry->AddInterface(base::BindRepeating(CreateResourceUsageReporter),
                           base::ThreadTaskRunnerHandle::Get());
  }
#endif  // !defined(OS_ANDROID)

  content::ServiceManagerConnection* connection = GetServiceManagerConnection();
  if (connection) {
    connection->AddConnectionFilter(
        std::make_unique<SimpleConnectionFilter>(std::move(registry)));
  }

  GetContentClient()->utility()->UtilityThreadStarted();

  service_factory_.reset(new UtilityServiceFactory);

  if (connection) {
    // NOTE: You must register any ConnectionFilter instances on |connection|
    // *before* this call to |Start()|, otherwise incoming interface requests
    // may race with the registration.
    connection->Start();
  }
}

bool UtilityThreadImpl::OnControlMessageReceived(const IPC::Message& msg) {
  return GetContentClient()->utility()->OnMessageReceived(msg);
}

void UtilityThreadImpl::RunService(
    const std::string& service_name,
    mojo::PendingReceiver<service_manager::mojom::Service> receiver) {
  DCHECK(service_factory_);
  service_factory_->RunService(service_name, std::move(receiver));
}

}  // namespace content
