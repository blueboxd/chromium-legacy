// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_CDM_SERVICE_FUCHSIA_CDM_MANAGER_H_
#define MEDIA_FUCHSIA_CDM_SERVICE_FUCHSIA_CDM_MANAGER_H_

#include <fuchsia/media/drm/cpp/fidl.h>
#include <string>

#include "media/base/provision_fetcher.h"

namespace url {
class Origin;
}  // namespace url

namespace media {

// Create and connect to Fuchsia CDM service. It will provision the origin if
// needed. It will chain all the concurrent provision requests and make sure we
// only send one provision request to server.
class FuchsiaCdmManager {
 public:
  FuchsiaCdmManager();
  ~FuchsiaCdmManager();

  void CreateAndProvision(
      const std::string& key_system,
      const url::Origin& origin,
      CreateFetcherCB create_fetcher_cb,
      fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
          request);
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_CDM_SERVICE_FUCHSIA_CDM_MANAGER_H_
