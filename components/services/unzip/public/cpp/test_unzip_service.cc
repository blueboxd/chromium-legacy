// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/unzip/public/cpp/test_unzip_service.h"

#include "components/services/filesystem/public/mojom/directory.mojom.h"

namespace unzip {

CrashyUnzipService::CrashyUnzipService(
    service_manager::mojom::ServiceRequest request)
    : binding_(this, std::move(request)) {}

CrashyUnzipService::~CrashyUnzipService() = default;

void CrashyUnzipService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  DCHECK(interface_name == mojom::Unzipper::Name_);
  DCHECK(!unzipper_binding_);
  unzipper_binding_ = std::make_unique<mojo::Binding<mojom::Unzipper>>(
      this, mojom::UnzipperRequest(std::move(interface_pipe)));
}

void CrashyUnzipService::Unzip(base::File zip_file,
                               filesystem::mojom::DirectoryPtr output_dir,
                               UnzipCallback callback) {
  unzipper_binding_.reset();
}

void CrashyUnzipService::UnzipWithFilter(
    base::File zip_file,
    filesystem::mojom::DirectoryPtr output_dir,
    mojom::UnzipFilterPtr filter,
    UnzipWithFilterCallback callback) {
  unzipper_binding_.reset();
}

}  // namespace unzip
