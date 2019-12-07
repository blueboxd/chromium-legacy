// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/utility_service_factory.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/utility/content_utility_client.h"
#include "content/public/utility/utility_thread.h"
#include "content/utility/utility_thread_impl.h"
#include "media/media_buildflags.h"
#include "services/service_manager/public/mojom/service.mojom.h"

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "media/cdm/cdm_adapter_factory.h"           // nogncheck
#include "media/mojo/mojom/constants.mojom.h"   // nogncheck
#include "media/mojo/services/cdm_service.h"         // nogncheck
#include "media/mojo/services/mojo_cdm_helper.h"     // nogncheck
#include "media/mojo/services/mojo_media_client.h"   // nogncheck
#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
#include "media/cdm/cdm_host_file.h"
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
#endif

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox.h"

extern sandbox::TargetServices* g_utility_target_services;
#endif

namespace content {

namespace {

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)

std::unique_ptr<media::CdmAuxiliaryHelper> CreateCdmHelper(
    service_manager::mojom::InterfaceProvider* interface_provider) {
  return std::make_unique<media::MojoCdmHelper>(interface_provider);
}

class ContentCdmServiceClient final : public media::CdmService::Client {
 public:
  ContentCdmServiceClient() {}
  ~ContentCdmServiceClient() override {}

  void EnsureSandboxed() override {
#if defined(OS_WIN)
    // |g_utility_target_services| can be null if --no-sandbox is specified.
    if (g_utility_target_services)
      g_utility_target_services->LowerToken();
#endif
  }

  std::unique_ptr<media::CdmFactory> CreateCdmFactory(
      service_manager::mojom::InterfaceProvider* host_interfaces) override {
    return std::make_unique<media::CdmAdapterFactory>(
        base::BindRepeating(&CreateCdmHelper, host_interfaces));
  }

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
  void AddCdmHostFilePaths(
      std::vector<media::CdmHostFilePath>* cdm_host_file_paths) override {
    GetContentClient()->AddContentDecryptionModules(nullptr,
                                                    cdm_host_file_paths);
  }
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
};

#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

}  // namespace

UtilityServiceFactory::UtilityServiceFactory() = default;

UtilityServiceFactory::~UtilityServiceFactory() = default;

void UtilityServiceFactory::RunService(
    const std::string& service_name,
    mojo::PendingReceiver<service_manager::mojom::Service> receiver) {
  auto request = service_manager::mojom::ServiceRequest(std::move(receiver));
  auto* trace_log = base::trace_event::TraceLog::GetInstance();
  if (trace_log->IsProcessNameEmpty())
    trace_log->set_process_name("Service: " + service_name);

  static auto* service_name_crash_key = base::debug::AllocateCrashKeyString(
      "service-name", base::debug::CrashKeySize::Size32);
  base::debug::SetCrashKeyString(service_name_crash_key, service_name);

  std::unique_ptr<service_manager::Service> service;
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  if (service_name == media::mojom::kCdmServiceName) {
    service = std::make_unique<media::CdmService>(
        std::make_unique<ContentCdmServiceClient>(), std::move(request));
  }
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

  if (service) {
    service_manager::Service::RunAsyncUntilTermination(
        std::move(service),
        base::BindOnce(&UtilityThread::ReleaseProcess,
                       base::Unretained(UtilityThread::Get())));
    return;
  }

  if (GetContentClient()->utility()->HandleServiceRequest(service_name,
                                                          std::move(request))) {
    return;
  }

  // Nothing knew how to handle this request. Complain loudly and die.
  LOG(ERROR) << "Ignoring request to start unknown service: " << service_name;
  UtilityThreadImpl* utility_thread =
      static_cast<UtilityThreadImpl*>(UtilityThread::Get());
  utility_thread->Shutdown();
  utility_thread->ReleaseProcess();
}

}  // namespace content
