// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_BLOOM_BLOOM_CONTROLLER_IMPL_H_
#define CHROMEOS_COMPONENTS_BLOOM_BLOOM_CONTROLLER_IMPL_H_

#include "chromeos/components/bloom/public/cpp/bloom_controller.h"

namespace ash {
class AssistantInteractionController;
}  // namespace ash

namespace signin {
class IdentityManager;
}  // namespace signin

namespace chromeos {
namespace bloom {

class BloomInteraction;
class ScreenshotGrabber;

class BloomControllerImpl : public BloomController {
 public:
  BloomControllerImpl(
      signin::IdentityManager* identity_manager,
      ash::AssistantInteractionController* assistant_interaction_controller);
  BloomControllerImpl(const BloomControllerImpl&) = delete;
  BloomControllerImpl& operator=(const BloomControllerImpl&) = delete;
  ~BloomControllerImpl() override;

  // BloomController implementation:
  void StartInteraction() override;
  BloomInteractionResolution GetLastInteractionResolution() const override;

  bool HasInteraction() const;
  void StopInteraction(BloomInteractionResolution resolution);

  signin::IdentityManager* identity_manager() { return identity_manager_; }
  ash::AssistantInteractionController* assistant_interaction_controller() {
    return assistant_interaction_controller_;
  }

 private:
  signin::IdentityManager* const identity_manager_;
  ash::AssistantInteractionController* const assistant_interaction_controller_;

  BloomInteractionResolution last_interaction_resolution_ =
      BloomInteractionResolution::kNormal;
};

}  // namespace bloom
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_BLOOM_BLOOM_CONTROLLER_IMPL_H_
