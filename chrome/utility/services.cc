// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/services.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/safe_browsing/buildflags.h"
#include "components/services/patch/file_patcher_impl.h"
#include "components/services/patch/public/mojom/file_patcher.mojom.h"
#include "components/services/unzip/public/mojom/unzipper.mojom.h"
#include "components/services/unzip/unzipper_impl.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/service_factory.h"
#include "printing/buildflags/buildflags.h"

#if defined(OS_WIN)
#include "chrome/services/util_win/public/mojom/util_win.mojom.h"
#include "chrome/services/util_win/util_win_impl.h"
#endif  // defined(OS_WIN)

#if !defined(OS_ANDROID)
#include "chrome/common/importer/profile_import.mojom.h"
#include "chrome/utility/importer/profile_import_impl.h"
#include "services/proxy_resolver/proxy_resolver_factory_impl.h"  // nogncheck
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"
#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_PRINTING) && defined(OS_CHROMEOS)
#include "chrome/services/ipp_parser/ipp_parser.h"  // nogncheck
#include "chrome/services/ipp_parser/public/mojom/ipp_parser.mojom.h"  // nogncheck
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING) || defined(OS_CHROMEOS)
#include "chrome/services/file_util/file_util_service.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/services/removable_storage_writer/public/mojom/removable_storage_writer.mojom.h"
#include "chrome/services/removable_storage_writer/removable_storage_writer.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS) || defined(OS_ANDROID)
#include "chrome/services/media_gallery_util/media_parser_factory.h"
#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom.h"
#endif

#if BUILDFLAG(ENABLE_VR) && !defined(OS_ANDROID)
#include "chrome/services/isolated_xr_device/xr_device_service.h"  // nogncheck
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"      // nogncheck
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) || \
    (BUILDFLAG(ENABLE_PRINTING) && defined(OS_WIN))
#include "chrome/services/printing/printing_service.h"
#include "chrome/services/printing/public/mojom/printing_service.mojom.h"
#endif

namespace {

auto RunFilePatcher(mojo::PendingReceiver<patch::mojom::FilePatcher> receiver) {
  return std::make_unique<patch::FilePatcherImpl>(std::move(receiver));
}

auto RunUnzipper(mojo::PendingReceiver<unzip::mojom::Unzipper> receiver) {
  return std::make_unique<unzip::UnzipperImpl>(std::move(receiver));
}

#if defined(OS_WIN)
auto RunWindowsUtility(mojo::PendingReceiver<chrome::mojom::UtilWin> receiver) {
  return std::make_unique<UtilWinImpl>(std::move(receiver));
}
#endif  // defined(OS_WIN)

#if !defined(OS_ANDROID)
auto RunProxyResolver(
    mojo::PendingReceiver<proxy_resolver::mojom::ProxyResolverFactory>
        receiver) {
  return std::make_unique<proxy_resolver::ProxyResolverFactoryImpl>(
      std::move(receiver));
}

auto RunProfileImporter(
    mojo::PendingReceiver<chrome::mojom::ProfileImport> receiver) {
  return std::make_unique<ProfileImportImpl>(std::move(receiver));
}
#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_PRINTING) && defined(OS_CHROMEOS)
auto RunCupsIppParser(
    mojo::PendingReceiver<ipp_parser::mojom::IppParser> receiver) {
  return std::make_unique<ipp_parser::IppParser>(std::move(receiver));
}
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING) || defined(OS_CHROMEOS)
auto RunFileUtil(
    mojo::PendingReceiver<chrome::mojom::FileUtilService> receiver) {
  return std::make_unique<FileUtilService>(std::move(receiver));
}
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
auto RunRemovableStorageWriter(
    mojo::PendingReceiver<chrome::mojom::RemovableStorageWriter> receiver) {
  return std::make_unique<RemovableStorageWriter>(std::move(receiver));
}
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS) || defined(OS_ANDROID)
auto RunMediaParserFactory(
    mojo::PendingReceiver<chrome::mojom::MediaParserFactory> receiver) {
  return std::make_unique<MediaParserFactory>(std::move(receiver));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) || defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_VR) && !defined(OS_ANDROID)
auto RunXrDeviceService(
    mojo::PendingReceiver<device::mojom::XRDeviceService> receiver) {
  return std::make_unique<device::XrDeviceService>(std::move(receiver));
}
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) || \
    (BUILDFLAG(ENABLE_PRINTING) && defined(OS_WIN))
auto RunPrintingService(
    mojo::PendingReceiver<printing::mojom::PrintingService> receiver) {
  return std::make_unique<printing::PrintingService>(std::move(receiver));
}
#endif

}  // namespace

mojo::ServiceFactory* GetElevatedMainThreadServiceFactory() {
  // NOTE: This ServiceFactory is only used in utility processes which are run
  // with elevated system privileges.
  // clang-format off
  static base::NoDestructor<mojo::ServiceFactory> factory {
#if BUILDFLAG(ENABLE_EXTENSIONS) && defined(OS_WIN)
    // On non-Windows, this service runs in a regular utility process.
    RunRemovableStorageWriter,
#endif
  };
  // clang-format on
  return factory.get();
}

mojo::ServiceFactory* GetMainThreadServiceFactory() {
  // clang-format off
  static base::NoDestructor<mojo::ServiceFactory> factory {
    RunFilePatcher,
    RunUnzipper,

#if !defined(OS_ANDROID)
    RunProfileImporter,
#endif

#if defined(OS_WIN)
    RunWindowsUtility,
#endif  // defined(OS_WIN)

#if BUILDFLAG(ENABLE_PRINTING) && defined(OS_CHROMEOS)
    RunCupsIppParser,
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING) || defined(OS_CHROMEOS)
    RunFileUtil,
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS) && !defined(OS_WIN)
    // On Windows, this service runs in an elevated utility process.
    RunRemovableStorageWriter,
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS) || defined(OS_ANDROID)
    RunMediaParserFactory,
#endif

#if BUILDFLAG(ENABLE_VR) && !defined(OS_ANDROID)
    RunXrDeviceService,
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) || \
    (BUILDFLAG(ENABLE_PRINTING) && defined(OS_WIN))
    RunPrintingService,
#endif
  };
  // clang-format on
  return factory.get();
}

mojo::ServiceFactory* GetIOThreadServiceFactory() {
  // clang-format off
  static base::NoDestructor<mojo::ServiceFactory> factory {
#if !defined(OS_ANDROID)
    RunProxyResolver,
#endif  // !defined(OS_ANDROID)
  };
  // clang-format on
  return factory.get();
}
