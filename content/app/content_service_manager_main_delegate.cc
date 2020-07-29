// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/content_service_manager_main_delegate.h"

#include "base/command_line.h"
#include "content/app/content_main_runner_impl.h"
#include "content/common/mojo_core_library_support.h"
#include "content/public/app/content_main_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/dynamic_library_support.h"
#include "services/service_manager/embedder/switches.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace content {

ContentServiceManagerMainDelegate::ContentServiceManagerMainDelegate(
    const ContentMainParams& params)
    : content_main_params_(params),
      content_main_runner_(ContentMainRunnerImpl::Create()) {}

ContentServiceManagerMainDelegate::~ContentServiceManagerMainDelegate() =
    default;

int ContentServiceManagerMainDelegate::Initialize(
    const InitializeParams& params) {
#if defined(OS_ANDROID)
  // May be called twice on Android due to the way browser startup requests are
  // dispatched by the system.
  if (initialized_)
    return -1;
#endif

#if defined(OS_MAC)
  content_main_params_.autorelease_pool = params.autorelease_pool;
#endif

  return content_main_runner_->Initialize(content_main_params_);
}

bool ContentServiceManagerMainDelegate::IsEmbedderSubprocess() {
  auto type = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kProcessType);
  return type == switches::kGpuProcess ||
         type == switches::kPpapiBrokerProcess ||
         type == switches::kPpapiPluginProcess ||
         type == switches::kRendererProcess ||
         type == switches::kUtilityProcess ||
         type == service_manager::switches::kZygoteProcess;
}

int ContentServiceManagerMainDelegate::RunEmbedderProcess() {
  return content_main_runner_->Run(start_service_manager_only_);
}

void ContentServiceManagerMainDelegate::ShutDownEmbedderProcess() {
#if !defined(OS_ANDROID)
  content_main_runner_->Shutdown();
#endif
}

service_manager::ProcessType
ContentServiceManagerMainDelegate::OverrideProcessType() {
  return content_main_params_.delegate->OverrideProcessType();
}

void ContentServiceManagerMainDelegate::InitializeMojo(
    mojo::core::Configuration* config) {
  // If this is the browser process and there's no Mojo invitation pipe on the
  // command line, we will serve as the global Mojo broker.
  const auto& command_line = *base::CommandLine::ForCurrentProcess();
  const bool is_browser = !command_line.HasSwitch(switches::kProcessType);
  if (is_browser) {
    if (mojo::PlatformChannel::CommandLineHasPassedEndpoint(command_line)) {
      config->is_broker_process = false;
      config->force_direct_shared_memory_allocation = true;
    } else {
      config->is_broker_process = true;
    }
  } else {
#if defined(OS_WIN)
    if (base::win::GetVersion() >= base::win::Version::WIN8_1) {
      // On Windows 8.1 and later it's not necessary to broker shared memory
      // allocation, as even sandboxed processes can allocate their own without
      // trouble.
      config->force_direct_shared_memory_allocation = true;
    }
#endif
  }

  if (!IsMojoCoreSharedLibraryEnabled()) {
    mojo::core::Init(*config);
    return;
  }

  if (!is_browser) {
    // Note that when dynamic Mojo Core is used, initialization for child
    // processes happens elsewhere. See ContentMainRunnerImpl::Run() and
    // ChildProcess construction.
    return;
  }

  MojoInitializeFlags flags = MOJO_INITIALIZE_FLAG_NONE;
  if (config->is_broker_process)
    flags |= MOJO_INITIALIZE_FLAG_AS_BROKER;
  if (config->force_direct_shared_memory_allocation)
    flags |= MOJO_INITIALIZE_FLAG_FORCE_DIRECT_SHARED_MEMORY_ALLOCATION;
  MojoResult result =
      mojo::LoadAndInitializeCoreLibrary(GetMojoCoreSharedLibraryPath(), flags);
  CHECK_EQ(MOJO_RESULT_OK, result);
}

std::vector<service_manager::Manifest>
ContentServiceManagerMainDelegate::GetServiceManifests() {
  return std::vector<service_manager::Manifest>();
}

bool ContentServiceManagerMainDelegate::ShouldLaunchAsServiceProcess(
    const service_manager::Identity& identity) {
  return identity.name() != mojom::kPackagedServicesServiceName;
}

void ContentServiceManagerMainDelegate::AdjustServiceProcessCommandLine(
    const service_manager::Identity& identity,
    base::CommandLine* command_line) {
  base::CommandLine::StringVector args_without_switches;
  if (identity.name() == mojom::kPackagedServicesServiceName) {
    // Ensure other arguments like URL are not lost.
    args_without_switches = command_line->GetArgs();

    // When launching the browser process, ensure that we don't inherit any
    // process type flag. When content embeds Service Manager, a process with no
    // type is launched as a browser process.
    base::CommandLine::SwitchMap switches = command_line->GetSwitches();
    switches.erase(switches::kProcessType);
    *command_line = base::CommandLine(command_line->GetProgram());
    for (const auto& sw : switches)
      command_line->AppendSwitchNative(sw.first, sw.second);
  }

  content_main_params_.delegate->AdjustServiceProcessCommandLine(identity,
                                                                 command_line);

  // Append other arguments back to |command_line| after the second call to
  // delegate as long as it can still remove all the arguments without switches.
  for (const auto& arg : args_without_switches)
    command_line->AppendArgNative(arg);
}

void ContentServiceManagerMainDelegate::OnServiceManagerInitialized(
    base::OnceClosure quit_closure,
    service_manager::BackgroundServiceManager* service_manager) {
  return content_main_params_.delegate->OnServiceManagerInitialized(
      std::move(quit_closure), service_manager);
}

std::unique_ptr<service_manager::Service>
ContentServiceManagerMainDelegate::CreateEmbeddedService(
    const std::string& service_name) {
  // TODO

  return nullptr;
}

void ContentServiceManagerMainDelegate::SetStartServiceManagerOnly(
    bool start_service_manager_only) {
  start_service_manager_only_ = start_service_manager_only;
}

}  // namespace content
