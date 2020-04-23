// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_UI_CONTROLLER_H_
#define ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_UI_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/optional.h"

namespace chromeos {
namespace assistant {
namespace mojom {
enum class AssistantEntryPoint;
enum class AssistantExitPoint;
}  // namespace mojom
}  // namespace assistant
}  // namespace chromeos

namespace ash {

class AssistantUiModel;
class AssistantUiModelObserver;

// The interface for the Assistant controller in charge of UI.
class ASH_PUBLIC_EXPORT AssistantUiController {
 public:
  // Returns the singleton instance owned by AssistantController.
  static AssistantUiController* Get();

  // Returns a pointer to the underlying model.
  virtual const AssistantUiModel* GetModel() const = 0;

  // Adds/removes the specified model observer.
  virtual void AddModelObserver(AssistantUiModelObserver*) = 0;
  virtual void RemoveModelObserver(AssistantUiModelObserver*) = 0;

  // Invoke to show/close/toggle Assistant UI.
  virtual void ShowUi(chromeos::assistant::mojom::AssistantEntryPoint) = 0;
  virtual void CloseUi(chromeos::assistant::mojom::AssistantExitPoint) = 0;
  virtual void ToggleUi(
      base::Optional<chromeos::assistant::mojom::AssistantEntryPoint>,
      base::Optional<chromeos::assistant::mojom::AssistantExitPoint>) = 0;

 protected:
  AssistantUiController();
  virtual ~AssistantUiController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_UI_CONTROLLER_H_
