// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/services.h"

#include <utility>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/services/patch/file_patcher_impl.h"
#include "components/services/patch/public/mojom/file_patcher.mojom.h"
#include "components/services/unzip/public/mojom/unzipper.mojom.h"
#include "components/services/unzip/unzipper_impl.h"
#include "mojo/public/cpp/bindings/service_factory.h"
#include "printing/buildflags/buildflags.h"

#if defined(OS_WIN)
#include "chrome/services/util_win/public/mojom/util_win.mojom.h"
#include "chrome/services/util_win/util_win_impl.h"
#endif  // defined(OS_WIN)

#if !defined(OS_ANDROID)
#include "services/proxy_resolver/proxy_resolver_factory_impl.h"  // nogncheck
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"
#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_PRINTING) && defined(OS_CHROMEOS)
#include "chrome/services/cups_ipp_parser/ipp_parser.h"  // nogncheck
#include "chrome/services/cups_ipp_parser/public/mojom/ipp_parser.mojom.h"  // nogncheck
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
#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_PRINTING) && defined(OS_CHROMEOS)
auto RunCupsIppParser(
    mojo::PendingReceiver<cups_ipp_parser::mojom::IppParser> receiver) {
  return std::make_unique<cups_ipp_parser::IppParser>(std::move(receiver));
}
#endif

}  // namespace

mojo::ServiceFactory* GetMainThreadServiceFactory() {
  // clang-format off
  static base::NoDestructor<mojo::ServiceFactory> factory {
    RunFilePatcher,
    RunUnzipper,

#if defined(OS_WIN)
    RunWindowsUtility,
#endif  // defined(OS_WIN)

#if BUILDFLAG(ENABLE_PRINTING) && defined(OS_CHROMEOS)
    RunCupsIppParser,
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
