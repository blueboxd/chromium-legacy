// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_CDM_FUCHSIA_CDM_FACTORY_H_
#define MEDIA_FUCHSIA_CDM_FUCHSIA_CDM_FACTORY_H_

#include "base/macros.h"
#include "media/base/cdm_factory.h"
#include "media/base/media_export.h"
#include "media/fuchsia/mojom/fuchsia_cdm_provider.mojom.h"

namespace service_manager {
class InterfaceProvider;
}

namespace media {

class MEDIA_EXPORT FuchsiaCdmFactory : public CdmFactory {
 public:
  // |interface_provider| must outlive this class.
  explicit FuchsiaCdmFactory(
      service_manager::InterfaceProvider* interface_provider);
  ~FuchsiaCdmFactory() final;

  // CdmFactory implementation.
  void Create(const std::string& key_system,
              const url::Origin& security_origin,
              const CdmConfig& cdm_config,
              const SessionMessageCB& session_message_cb,
              const SessionClosedCB& session_closed_cb,
              const SessionKeysChangeCB& session_keys_change_cb,
              const SessionExpirationUpdateCB& session_expiration_update_cb,
              const CdmCreatedCB& cdm_created_cb) final;

 private:
  service_manager::InterfaceProvider* const interface_provider_;
  media::mojom::FuchsiaCdmProviderPtr cdm_provider_;

  DISALLOW_COPY_AND_ASSIGN(FuchsiaCdmFactory);
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_CDM_FUCHSIA_CDM_FACTORY_H_
