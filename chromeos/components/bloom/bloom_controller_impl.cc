// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/bloom/bloom_controller_impl.h"

#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"

namespace chromeos {
namespace bloom {

BloomControllerImpl::BloomControllerImpl(
    signin::IdentityManager* identity_manager,
    ash::AssistantInteractionController* assistant_interaction_controller)
    : identity_manager_(identity_manager),
      assistant_interaction_controller_(assistant_interaction_controller) {
  DCHECK(identity_manager_);
  DCHECK(assistant_interaction_controller_);
}

BloomControllerImpl::~BloomControllerImpl() = default;

void BloomControllerImpl::StartInteraction() {
  // TODO(jeroendh): Implement
}

bool BloomControllerImpl::HasInteraction() const {
  // TODO(jeroendh): Implement
  return true;
}

void BloomControllerImpl::StopInteraction(
    BloomInteractionResolution resolution) {
  // TODO(jeroendh): Implement
  last_interaction_resolution_ = resolution;
}

BloomInteractionResolution BloomControllerImpl::GetLastInteractionResolution()
    const {
  return last_interaction_resolution_;
}

}  // namespace bloom
}  // namespace chromeos
