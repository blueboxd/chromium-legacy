// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/file_util_service.h"

#include <memory>

#include "base/bind.h"
#include "build/build_config.h"
#include "components/safe_browsing/buildflags.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/services/file_util/safe_archive_analyzer.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/services/file_util/zip_file_creator.h"
#endif

namespace {

#if BUILDFLAG(FULL_SAFE_BROWSING)
void OnSafeArchiveAnalyzerRequest(
    service_manager::ServiceKeepalive* keepalive,
    chrome::mojom::SafeArchiveAnalyzerRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<SafeArchiveAnalyzer>(keepalive->CreateRef()),
      std::move(request));
}
#endif

#if defined(OS_CHROMEOS)
void OnZipFileCreatorRequest(service_manager::ServiceKeepalive* keepalive,
                             chrome::mojom::ZipFileCreatorRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<chrome::ZipFileCreator>(keepalive->CreateRef()),
      std::move(request));
}
#endif

}  // namespace

FileUtilService::FileUtilService(service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)),
      service_keepalive_(&service_binding_, base::TimeDelta()) {}

FileUtilService::~FileUtilService() = default;

void FileUtilService::OnStart() {
#if defined(OS_CHROMEOS)
  registry_.AddInterface(
      base::BindRepeating(&OnZipFileCreatorRequest, &service_keepalive_));
#endif
#if BUILDFLAG(FULL_SAFE_BROWSING)
  registry_.AddInterface(
      base::BindRepeating(&OnSafeArchiveAnalyzerRequest, &service_keepalive_));
#endif
}

void FileUtilService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}
