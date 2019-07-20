// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/file/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "components/services/leveldb/public/mojom/leveldb.mojom.h"
#include "services/file/public/mojom/constants.mojom.h"
#include "services/file/public/mojom/file_system.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace file {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("File Service")
          .ExposeCapability("file:leveldb",
                            service_manager::Manifest::InterfaceList<
                                leveldb::mojom::LevelDBService>())
          .ExposeCapability(
              "file:filesystem",
              service_manager::Manifest::InterfaceList<mojom::FileSystem>())
          .Build()};
  return *manifest;
}

}  // namespace file
