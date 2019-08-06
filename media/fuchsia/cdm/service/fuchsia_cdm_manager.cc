// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/cdm/service/fuchsia_cdm_manager.h"

#include "base/logging.h"
#include "url/origin.h"

namespace media {

FuchsiaCdmManager::FuchsiaCdmManager() = default;

FuchsiaCdmManager::~FuchsiaCdmManager() = default;

void FuchsiaCdmManager::CreateAndProvision(
    const std::string& key_system,
    const url::Origin& origin,
    CreateFetcherCB create_fetcher_cb,
    fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
        request) {
  NOTIMPLEMENTED();
}

}  // namespace media
