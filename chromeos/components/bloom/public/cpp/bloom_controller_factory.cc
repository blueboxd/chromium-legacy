// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/bloom/public/cpp/bloom_controller_factory.h"

#include "base/callback.h"
#include "chromeos/components/bloom/bloom_controller_impl.h"

namespace chromeos {
namespace bloom {

// static
std::unique_ptr<BloomController> BloomControllerFactory::Create(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    ash::AssistantInteractionController* assistant_interaction_controller) {
  return std::make_unique<BloomControllerImpl>(
      identity_manager, assistant_interaction_controller);
}

}  // namespace bloom
}  // namespace chromeos
