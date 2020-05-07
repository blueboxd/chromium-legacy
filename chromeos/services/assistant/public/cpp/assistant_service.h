// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_SERVICE_H_
#define CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_SERVICE_H_

#include "base/component_export.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace chromeos {
namespace assistant {

// Main interface between browser and |chromeos::assistant::Service|.
class COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) AssistantService {
 public:
  AssistantService();
  AssistantService(const AssistantService&) = delete;
  AssistantService& operator=(const AssistantService&) = delete;
  virtual ~AssistantService();

  static AssistantService* Get();

  // Initiates assistant service.
  virtual void Init() = 0;

  // Binds the main assistant backend interface.
  virtual void BindAssistant(
      mojo::PendingReceiver<mojom::Assistant> receiver) = 0;

  // Signals system shutdown, the service could start cleaning up if needed.
  virtual void Shutdown() = 0;
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_SERVICE_H_
