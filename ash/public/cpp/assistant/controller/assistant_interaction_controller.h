// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_INTERACTION_CONTROLLER_H_
#define ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_INTERACTION_CONTROLLER_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom-forward.h"

namespace ash {

class AssistantInteractionModel;
class AssistantInteractionModelObserver;

// The interface for the Assistant controller in charge of interactions.
class ASH_PUBLIC_EXPORT AssistantInteractionController {
 public:
  // Returns the singleton instance owned by AssistantController.
  static AssistantInteractionController* Get();

  // Returns a pointer to the underlying model.
  virtual const AssistantInteractionModel* GetModel() const = 0;

  // Adds/removes the specified model observer.
  virtual void AddModelObserver(AssistantInteractionModelObserver*) = 0;
  virtual void RemoveModelObserver(AssistantInteractionModelObserver*) = 0;

  // Start Assistant text interaction.
  virtual void StartTextInteraction(
      const std::string& query,
      bool allow_tts,
      chromeos::assistant::mojom::AssistantQuerySource source) = 0;

 protected:
  AssistantInteractionController();
  virtual ~AssistantInteractionController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_INTERACTION_CONTROLLER_H_
