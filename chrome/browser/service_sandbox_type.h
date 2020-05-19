// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICE_SANDBOX_TYPE_H_
#define CHROME_BROWSER_SERVICE_SANDBOX_TYPE_H_

#include "build/build_config.h"
#include "content/public/browser/sandbox_type.h"
#include "content/public/browser/service_process_host.h"

// This file maps service classes to sandbox types.  Services which
// require a non-utility sandbox can be added here.  See
// ServiceProcessHost::Launch() for how these templates are consumed.

// chrome::mojom::RemovableStorageWriter
namespace chrome {
namespace mojom {
class RemovableStorageWriter;
}  // namespace mojom
}  // namespace chrome
template <>
inline content::SandboxType
content::GetServiceSandboxType<chrome::mojom::RemovableStorageWriter>() {
#if defined(OS_WIN)
  return SandboxType::kNoSandboxAndElevatedPrivileges;
#else
  return SandboxType::kNoSandbox;
#endif
}

// chrome::mojom::UtilWin
#if defined(OS_WIN)
namespace chrome {
namespace mojom {
class UtilWin;
}
}  // namespace chrome
template <>
inline content::SandboxType
content::GetServiceSandboxType<chrome::mojom::UtilWin>() {
  return content::SandboxType::kNoSandbox;
}
#endif

// chrome::mojom::ProfileImport
namespace chrome {
namespace mojom {
class ProfileImport;
}
}  // namespace chrome
template <>
inline content::SandboxType
content::GetServiceSandboxType<chrome::mojom::ProfileImport>() {
  return content::SandboxType::kNoSandbox;
}

// printing::mojom::PrintingService
#if defined(OS_WIN)
namespace printing {
namespace mojom {
class PrintingService;
}
}  // namespace printing
template <>
inline content::SandboxType
content::GetServiceSandboxType<printing::mojom::PrintingService>() {
  return content::SandboxType::kPdfConversion;
}
#endif  // defined(OS_WIN)

// proxy_resolver::mojom::ProxyResolverFactory
#if defined(OS_WIN)
namespace proxy_resolver {
namespace mojom {
class ProxyResolverFactory;
}
}  // namespace proxy_resolver
template <>
inline content::SandboxType
content::GetServiceSandboxType<proxy_resolver::mojom::ProxyResolverFactory>() {
  return content::SandboxType::kProxyResolver;
}
#endif  // defined(OS_WIN)

#endif  // CHROME_BROWSER_SERVICE_SANDBOX_TYPE_H_
