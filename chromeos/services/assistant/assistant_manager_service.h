// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_H_
#define CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_H_

#include <memory>
#include <string>

#include "ash/public/mojom/assistant_controller.mojom.h"
#include "base/callback_forward.h"
#include "base/component_export.h"
#include "chromeos/services/assistant/assistant_settings_manager.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "chromeos/services/assistant/public/mojom/settings.mojom.h"

namespace chromeos {
namespace assistant {
class AssistantCommunicationErrorObserver;

// Interface class that defines all assistant functionalities.
class COMPONENT_EXPORT(ASSISTANT_SERVICE) AssistantManagerService
    : public mojom::Assistant {
 public:
  enum State {
    // Initial state, the service is created but not started yet.
    STOPPED = 0,
    // The service is started, it takes a little time to be fully running.
    STARTED = 1,
    // The service is fully running and ready to take requests.
    RUNNING = 2
  };

  ~AssistantManagerService() override = default;

  // Start the assistant in the background with |access_token|, where the
  // token can be nullopt when the service is being started under the signed
  // out mode. When the service is fully started |callback| will be called on
  // the thread where ctor was run.
  virtual void Start(const base::Optional<std::string>& access_token,
                     bool enable_hotword,
                     base::OnceClosure callback) = 0;

  // Stop the assistant.
  virtual void Stop() = 0;

  // Returns the current state.
  virtual State GetState() const = 0;

  // Set access token for assistant.
  virtual void SetAccessToken(const std::string& access_token) = 0;

  // Turn on / off all listening, including hotword and voice query.
  virtual void EnableListening(bool enable) = 0;

  // Turn on / off hotword listening.
  virtual void EnableHotword(bool enable) = 0;

  virtual void SetArcPlayStoreEnabled(bool enabled) = 0;

  // Returns a pointer of AssistantSettingsManager.
  virtual AssistantSettingsManager* GetAssistantSettingsManager() = 0;

  // Add/Remove an observer that is invoked when there is a communucation error
  // with the Assistant service.
  virtual void AddCommunicationErrorObserver(
      AssistantCommunicationErrorObserver* observer) = 0;
  virtual void RemoveCommunicationErrorObserver(
      AssistantCommunicationErrorObserver* observer) = 0;
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_H_
