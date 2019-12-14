// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/builtin_service_manifests.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "content/public/app/content_browser_manifest.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"

namespace content {

const std::vector<service_manager::Manifest>& GetBuiltinServiceManifests() {
  static base::NoDestructor<std::vector<service_manager::Manifest>> manifests{
      std::vector<service_manager::Manifest>{GetContentBrowserManifest()}};
  return *manifests;
}

}  // namespace content
