// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/chrome_content_utility_client.h"

#include <stddef.h>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/common/buildflags.h"
#include "components/mirroring/mojom/constants.mojom.h"
#include "components/mirroring/service/features.h"
#include "components/mirroring/service/mirroring_service.h"
#include "components/services/patch/patch_service.h"
#include "components/services/patch/public/mojom/constants.mojom.h"
#include "components/services/unzip/public/mojom/constants.mojom.h"
#include "components/services/unzip/unzip_service.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/simple_connection_filter.h"
#include "content/public/utility/utility_thread.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/sandbox/switches.h"
#include "ui/base/buildflags.h"

#if !defined(OS_ANDROID)
#include "chrome/utility/importer/profile_import_impl.h"
#include "chrome/utility/importer/profile_import_service.h"
#include "services/network/url_request_context_builder_mojo.h"
#include "services/proxy_resolver/proxy_resolver_factory_impl.h"  // nogncheck
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"
#endif  // !defined(OS_ANDROID)

#if defined(OS_WIN)
#include "chrome/services/util_win/public/mojom/util_win.mojom.h"
#include "chrome/services/util_win/util_win_impl.h"
#include "components/services/quarantine/public/cpp/quarantine_features_win.h"  // nogncheck
#include "components/services/quarantine/public/mojom/quarantine.mojom.h"  // nogncheck
#include "components/services/quarantine/quarantine_service.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_VR) && !defined(OS_ANDROID)
#include "chrome/services/isolated_xr_device/xr_device_service.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/services/removable_storage_writer/public/mojom/constants.mojom.h"
#include "chrome/services/removable_storage_writer/removable_storage_writer_service.h"
#if defined(OS_WIN)
#include "chrome/services/wifi_util_win/public/mojom/constants.mojom.h"
#include "chrome/services/wifi_util_win/wifi_util_win_service.h"
#endif
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS) || defined(OS_ANDROID)
#include "chrome/services/media_gallery_util/media_gallery_util_service.h"
#include "chrome/services/media_gallery_util/public/mojom/constants.mojom.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/assistant/buildflags.h"  // nogncheck
#include "chromeos/services/ime/ime_service.h"
#include "chromeos/services/ime/public/mojom/constants.mojom.h"

#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#include "chromeos/services/assistant/audio_decoder/assistant_audio_decoder_service.h"  // nogncheck
#include "chromeos/services/assistant/public/mojom/constants.mojom.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/common/chrome_content_client.h"
#include "components/services/pdf_compositor/public/cpp/pdf_compositor_service_factory.h"  // nogncheck
#include "components/services/pdf_compositor/public/mojom/pdf_compositor.mojom.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) || \
    (BUILDFLAG(ENABLE_PRINTING) && defined(OS_WIN))
#include "chrome/services/printing/printing_service.h"
#include "chrome/services/printing/public/mojom/constants.mojom.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING) && defined(OS_WIN)
#include "chrome/services/printing/pdf_to_emf_converter_factory.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && defined(OS_WIN)
#include "chrome/utility/printing_handler.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING) && defined(OS_CHROMEOS)
#include "chrome/services/cups_ipp_parser/cups_ipp_parser_service.h"  // nogncheck
#include "chrome/services/cups_ipp_parser/public/mojom/constants.mojom.h"  // nogncheck
#endif

#if defined(FULL_SAFE_BROWSING) || defined(OS_CHROMEOS)
#include "chrome/services/file_util/file_util_service.h"  // nogncheck
#include "chrome/services/file_util/public/mojom/constants.mojom.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_SIMPLE_BROWSER_SERVICE_OUT_OF_PROCESS)
#include "services/content/simple_browser/public/mojom/constants.mojom.h"  // nogncheck
#include "services/content/simple_browser/simple_browser_service.h"  // nogncheck
#endif

namespace {

base::LazyInstance<ChromeContentUtilityClient::NetworkBinderCreationCallback>::
    Leaky g_network_binder_creation_callback = LAZY_INSTANCE_INITIALIZER;

void RunServiceAsyncThenTerminateProcess(
    std::unique_ptr<service_manager::Service> service) {
  service_manager::Service::RunAsyncUntilTermination(
      std::move(service),
      base::BindOnce([] { content::UtilityThread::Get()->ReleaseProcess(); }));
}

}  // namespace

ChromeContentUtilityClient::ChromeContentUtilityClient()
    : utility_process_running_elevated_(false) {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && defined(OS_WIN)
  printing_handler_ = std::make_unique<printing::PrintingHandler>();
#endif
}

ChromeContentUtilityClient::~ChromeContentUtilityClient() = default;

void ChromeContentUtilityClient::UtilityThreadStarted() {
#if defined(OS_WIN)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  utility_process_running_elevated_ = command_line->HasSwitch(
      service_manager::switches::kNoSandboxAndElevatedPrivileges);
#endif

  content::ServiceManagerConnection* connection =
      content::ChildThread::Get()->GetServiceManagerConnection();

  // NOTE: Some utility process instances are not connected to the Service
  // Manager. Nothing left to do in that case.
  if (!connection)
    return;

  auto registry = std::make_unique<service_manager::BinderRegistry>();
  // If our process runs with elevated privileges, only add elevated Mojo
  // interfaces to the interface registry.
  if (!utility_process_running_elevated_) {
#if BUILDFLAG(ENABLE_PRINTING) && defined(OS_WIN)
    // TODO(crbug.com/798782): remove when the Cloud print chrome/service is
    // removed.
    registry->AddInterface(
        base::BindRepeating(printing::PdfToEmfConverterFactory::Create),
        base::ThreadTaskRunnerHandle::Get());
#endif
  }

  connection->AddConnectionFilter(
      std::make_unique<content::SimpleConnectionFilter>(std::move(registry)));
}

bool ChromeContentUtilityClient::OnMessageReceived(
    const IPC::Message& message) {
  if (utility_process_running_elevated_)
    return false;

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && defined(OS_WIN)
  if (printing_handler_->OnMessageReceived(message))
    return true;
#endif
  return false;
}

bool ChromeContentUtilityClient::HandleServiceRequest(
    const std::string& service_name,
    service_manager::mojom::ServiceRequest request) {
  if (utility_process_running_elevated_) {
    // This process is running with elevated privileges. Only handle a limited
    // set of service requests in this case.
    auto service = MaybeCreateElevatedService(service_name, std::move(request));
    if (service) {
      RunServiceAsyncThenTerminateProcess(std::move(service));
      return true;
    }

    return false;
  }

  auto service = MaybeCreateMainThreadService(service_name, std::move(request));
  if (service) {
    RunServiceAsyncThenTerminateProcess(std::move(service));
    return true;
  }

  return false;
}

std::unique_ptr<service_manager::Service>
ChromeContentUtilityClient::MaybeCreateMainThreadService(
    const std::string& service_name,
    service_manager::mojom::ServiceRequest request) {
  if (service_name == unzip::mojom::kServiceName)
    return std::make_unique<unzip::UnzipService>(std::move(request));

  if (service_name == patch::mojom::kServiceName)
    return std::make_unique<patch::PatchService>(std::move(request));

#if BUILDFLAG(ENABLE_PRINTING)
  if (service_name == printing::mojom::kServiceName)
    return printing::CreatePdfCompositorService(std::move(request));
#endif

#if BUILDFLAG(ENABLE_VR) && !defined(OS_ANDROID)
  if (service_name == device::mojom::kVrIsolatedServiceName)
    return std::make_unique<device::XrDeviceService>(std::move(request));
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) || \
    (BUILDFLAG(ENABLE_PRINTING) && defined(OS_WIN))
  if (service_name == printing::mojom::kChromePrintingServiceName)
    return std::make_unique<printing::PrintingService>(std::move(request));
#endif

#if defined(OS_WIN)
  if (service_name == quarantine::mojom::kServiceName &&
      base::FeatureList::IsEnabled(quarantine::kOutOfProcessQuarantine)) {
    return std::make_unique<quarantine::QuarantineService>(std::move(request));
  }
#endif  // OS_WIN

#if !defined(OS_ANDROID)
  if (service_name == chrome::mojom::kProfileImportServiceName)
    return std::make_unique<ProfileImportService>(std::move(request));

  if (base::FeatureList::IsEnabled(mirroring::features::kMirroringService) &&
      base::FeatureList::IsEnabled(features::kAudioServiceAudioStreams) &&
      service_name == mirroring::mojom::kServiceName) {
    return std::make_unique<mirroring::MirroringService>(
        std::move(request), content::ChildThread::Get()->GetIOTaskRunner());
  }
#endif

#if defined(FULL_SAFE_BROWSING) || defined(OS_CHROMEOS)
  if (service_name == chrome::mojom::kFileUtilServiceName)
    return std::make_unique<FileUtilService>(std::move(request));
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS) && !defined(OS_WIN)
  // On Windows the service is running elevated.
  if (service_name == chrome::mojom::kRemovableStorageWriterServiceName)
    return std::make_unique<RemovableStorageWriterService>(std::move(request));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) && !defined(OS_WIN)

#if BUILDFLAG(ENABLE_EXTENSIONS) || defined(OS_ANDROID)
  if (service_name == chrome::mojom::kMediaGalleryUtilServiceName)
    return std::make_unique<MediaGalleryUtilService>(std::move(request));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) || defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_SIMPLE_BROWSER_SERVICE_OUT_OF_PROCESS)
  if (service_name == simple_browser::mojom::kServiceName) {
    return std::make_unique<simple_browser::SimpleBrowserService>(
        std::move(request), simple_browser::SimpleBrowserService::
                                UIInitializationMode::kInitializeUI);
  }
#endif

#if defined(OS_CHROMEOS)
  if (service_name == chromeos::ime::mojom::kServiceName)
    return std::make_unique<chromeos::ime::ImeService>(std::move(request));

#if BUILDFLAG(ENABLE_PRINTING)
  if (service_name == cups_ipp_parser::mojom::kCupsIppParserServiceName)
    return std::make_unique<cups_ipp_parser::CupsIppParserService>(
        std::move(request));
#endif

#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
  if (service_name == chromeos::assistant::mojom::kAudioDecoderServiceName) {
    return std::make_unique<chromeos::assistant::AssistantAudioDecoderService>(
        std::move(request));
  }
#endif

#endif  // defined(OS_CHROMEOS)
  return nullptr;
}

std::unique_ptr<service_manager::Service>
ChromeContentUtilityClient::MaybeCreateElevatedService(
    const std::string& service_name,
    service_manager::mojom::ServiceRequest request) {
  DCHECK(utility_process_running_elevated_);
#if defined(OS_WIN) && BUILDFLAG(ENABLE_EXTENSIONS)
  if (service_name == chrome::mojom::kWifiUtilWinServiceName)
    return std::make_unique<WifiUtilWinService>(std::move(request));

  if (service_name == chrome::mojom::kRemovableStorageWriterServiceName)
    return std::make_unique<RemovableStorageWriterService>(std::move(request));
#endif

  return nullptr;
}

void ChromeContentUtilityClient::RegisterNetworkBinders(
    service_manager::BinderRegistry* registry) {
  if (g_network_binder_creation_callback.Get())
    g_network_binder_creation_callback.Get().Run(registry);
}

void ChromeContentUtilityClient::RunMainThreadService(
    mojo::GenericPendingReceiver receiver) {
#if defined(OS_WIN)
  if (auto util_receiver = receiver.As<chrome::mojom::UtilWin>()) {
    static base::NoDestructor<UtilWinImpl> service(std::move(util_receiver));
  }
#endif
}

void ChromeContentUtilityClient::RunIOThreadService(
    mojo::GenericPendingReceiver* receiver) {
#if !defined(OS_ANDROID)
  if (auto factory_receiver =
          receiver->As<proxy_resolver::mojom::ProxyResolverFactory>()) {
    static base::NoDestructor<proxy_resolver::ProxyResolverFactoryImpl> factory(
        std::move(factory_receiver));
    return;
  }
#endif  // !defined(OS_ANDROID)
}

// static
void ChromeContentUtilityClient::SetNetworkBinderCreationCallback(
    const NetworkBinderCreationCallback& callback) {
  g_network_binder_creation_callback.Get() = callback;
}
