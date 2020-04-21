// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/services.h"

#include <utility>

#include "base/no_destructor.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/services/storage/public/mojom/storage_service.mojom.h"
#include "components/services/storage/storage_service_impl.h"
#include "content/child/child_process.h"
#include "content/public/utility/content_utility_client.h"
#include "content/public/utility/utility_thread.h"
#include "device/vr/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/service_factory.h"
#include "services/audio/service_factory.h"
#include "services/data_decoder/data_decoder_service.h"
#include "services/network/network_service.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/tracing/public/mojom/tracing_service.mojom.h"
#include "services/tracing/tracing_service.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"
#include "services/video_capture/video_capture_service_impl.h"

#if defined(OS_MACOSX)
#include "base/mac/mach_logging.h"
#include "sandbox/mac/system_services.h"
#include "services/service_manager/sandbox/features.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "media/cdm/cdm_adapter_factory.h"          // nogncheck
#include "media/mojo/services/cdm_service.h"        // nogncheck
#include "media/mojo/services/mojo_cdm_helper.h"    // nogncheck
#include "media/mojo/services/mojo_media_client.h"  // nogncheck
#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
#include "media/cdm/cdm_host_file.h"
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)

#if BUILDFLAG(ENABLE_VR) && !defined(OS_ANDROID)
#include "content/services/isolated_xr_device/xr_device_service.h"  // nogncheck
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"       // nogncheck
#endif

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "sandbox/win/src/sandbox.h"

extern sandbox::TargetServices* g_utility_target_services;
#endif  // defined(OS_WIN)
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

namespace content {

namespace {

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)

std::unique_ptr<media::CdmAuxiliaryHelper> CreateCdmHelper(
    media::mojom::FrameInterfaceFactory* interface_provider) {
  return std::make_unique<media::MojoCdmHelper>(interface_provider);
}

class ContentCdmServiceClient final : public media::CdmService::Client {
 public:
  ContentCdmServiceClient() = default;
  ~ContentCdmServiceClient() override = default;

  void EnsureSandboxed() override {
#if defined(OS_WIN)
    // |g_utility_target_services| can be null if --no-sandbox is specified.
    if (g_utility_target_services)
      g_utility_target_services->LowerToken();
#endif
  }

  std::unique_ptr<media::CdmFactory> CreateCdmFactory(
      media::mojom::FrameInterfaceFactory* frame_interfaces) override {
    return std::make_unique<media::CdmAdapterFactory>(
        base::BindRepeating(&CreateCdmHelper, frame_interfaces));
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

class UtilityThreadVideoCaptureServiceImpl final
    : public video_capture::VideoCaptureServiceImpl {
 public:
  explicit UtilityThreadVideoCaptureServiceImpl(
      mojo::PendingReceiver<video_capture::mojom::VideoCaptureService> receiver,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
      : VideoCaptureServiceImpl(std::move(receiver),
                                std::move(ui_task_runner)) {}

 private:
#if defined(OS_WIN)
  base::win::ScopedCOMInitializer com_initializer_{
      base::win::ScopedCOMInitializer::kMTA};
#endif
};

auto RunNetworkService(
    mojo::PendingReceiver<network::mojom::NetworkService> receiver) {
  auto binders = std::make_unique<service_manager::BinderRegistry>();
  GetContentClient()->utility()->RegisterNetworkBinders(binders.get());
  return std::make_unique<network::NetworkService>(
      std::move(binders), std::move(receiver),
      /*delay_initialization_until_set_client=*/true);
}

auto RunAudio(mojo::PendingReceiver<audio::mojom::AudioService> receiver) {
#if defined(OS_MACOSX)
  // Don't connect to launch services when running sandboxed
  // (https://crbug.com/874785).
  if (service_manager::IsAudioSandboxEnabled()) {
    sandbox::DisableLaunchServices();
  }

  // Set the audio process to run with similar scheduling parameters as the
  // browser process.
  task_category_policy category;
  category.role = TASK_FOREGROUND_APPLICATION;
  kern_return_t result = task_policy_set(
      mach_task_self(), TASK_CATEGORY_POLICY,
      reinterpret_cast<task_policy_t>(&category), TASK_CATEGORY_POLICY_COUNT);

  MACH_LOG_IF(ERROR, result != KERN_SUCCESS, result)
      << "task_policy_set TASK_CATEGORY_POLICY";

  task_qos_policy qos;
  qos.task_latency_qos_tier = LATENCY_QOS_TIER_0;
  qos.task_throughput_qos_tier = THROUGHPUT_QOS_TIER_0;
  result = task_policy_set(mach_task_self(), TASK_BASE_QOS_POLICY,
                           reinterpret_cast<task_policy_t>(&qos),
                           TASK_QOS_POLICY_COUNT);

  MACH_LOG_IF(ERROR, result != KERN_SUCCESS, result)
      << "task_policy_set TASK_QOS_POLICY";
#endif

  return audio::CreateStandaloneService(std::move(receiver));
}

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
auto RunCdmService(mojo::PendingReceiver<media::mojom::CdmService> receiver) {
  return std::make_unique<media::CdmService>(
      std::make_unique<ContentCdmServiceClient>(), std::move(receiver));
}
#endif

auto RunDataDecoder(
    mojo::PendingReceiver<data_decoder::mojom::DataDecoderService> receiver) {
  UtilityThread::Get()->EnsureBlinkInitialized();
  return std::make_unique<data_decoder::DataDecoderService>(
      std::move(receiver));
}

auto RunStorageService(
    mojo::PendingReceiver<storage::mojom::StorageService> receiver) {
  return std::make_unique<storage::StorageServiceImpl>(
      std::move(receiver), ChildProcess::current()->io_task_runner());
}

auto RunTracing(
    mojo::PendingReceiver<tracing::mojom::TracingService> receiver) {
  return std::make_unique<tracing::TracingService>(std::move(receiver));
}

auto RunVideoCapture(
    mojo::PendingReceiver<video_capture::mojom::VideoCaptureService> receiver) {
  return std::make_unique<UtilityThreadVideoCaptureServiceImpl>(
      std::move(receiver), base::ThreadTaskRunnerHandle::Get());
}

#if BUILDFLAG(ENABLE_VR) && !defined(OS_ANDROID)
auto RunXrDeviceService(
    mojo::PendingReceiver<device::mojom::XRDeviceService> receiver) {
  return std::make_unique<device::XrDeviceService>(std::move(receiver));
}
#endif

mojo::ServiceFactory& GetIOThreadServiceFactory() {
  static base::NoDestructor<mojo::ServiceFactory> factory{
      RunNetworkService,
  };
  return *factory;
}

mojo::ServiceFactory& GetMainThreadServiceFactory() {
  // clang-format off
  static base::NoDestructor<mojo::ServiceFactory> factory{
    RunAudio,
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
    RunCdmService,
#endif
    RunDataDecoder,
    RunStorageService,
    RunTracing,
    RunVideoCapture,
#if BUILDFLAG(ENABLE_VR) && !defined(OS_ANDROID)
    RunXrDeviceService,
#endif
  };
  // clang-format on
  return *factory;
}

}  // namespace

void HandleServiceRequestOnIOThread(
    mojo::GenericPendingReceiver receiver,
    base::SequencedTaskRunner* main_thread_task_runner) {
  if (GetIOThreadServiceFactory().MaybeRunService(&receiver))
    return;

  // If the request was handled already, we should not reach this point.
  DCHECK(receiver.is_valid());
  auto* embedder_factory =
      GetContentClient()->utility()->GetIOThreadServiceFactory();
  if (embedder_factory && embedder_factory->MaybeRunService(&receiver))
    return;

  DCHECK(receiver.is_valid());
  main_thread_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&HandleServiceRequestOnMainThread, std::move(receiver)));
}

void HandleServiceRequestOnMainThread(mojo::GenericPendingReceiver receiver) {
  if (GetMainThreadServiceFactory().MaybeRunService(&receiver))
    return;

  // If the request was handled already, we should not reach this point.
  DCHECK(receiver.is_valid());
  auto* embedder_factory =
      GetContentClient()->utility()->GetMainThreadServiceFactory();
  if (embedder_factory && embedder_factory->MaybeRunService(&receiver))
    return;

  DCHECK(receiver.is_valid());
  DLOG(ERROR) << "Unhandled out-of-process service request for "
              << receiver.interface_name().value();
}

}  // namespace content
