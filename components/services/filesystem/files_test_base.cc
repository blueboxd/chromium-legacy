// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/filesystem/files_test_base.h"

#include <utility>

#include "components/services/filesystem/public/cpp/manifest.h"
#include "components/services/filesystem/public/mojom/directory.mojom.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace filesystem {

const char kTestServiceName[] = "filesystem_service_unittests";

FilesTestBase::FilesTestBase()
    : test_service_manager_(
          {GetManifest(),
           service_manager::ManifestBuilder()
               .WithServiceName(kTestServiceName)
               .RequireCapability("filesystem", "filesystem:filesystem")
               .Build()}),
      test_service_(
          test_service_manager_.RegisterTestInstance(kTestServiceName)) {}

FilesTestBase::~FilesTestBase() = default;

void FilesTestBase::SetUp() {
  connector()->Connect("filesystem", files_.BindNewPipeAndPassReceiver());
}

void FilesTestBase::GetTemporaryRoot(
    mojo::Remote<mojom::Directory>* directory) {
  base::File::Error error = base::File::Error::FILE_ERROR_FAILED;
  bool handled = files()->OpenTempDirectory(
      directory->BindNewPipeAndPassReceiver(), &error);
  ASSERT_TRUE(handled);
  ASSERT_EQ(base::File::Error::FILE_OK, error);
}

}  // namespace filesystem
