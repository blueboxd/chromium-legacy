// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/public/cpp/default_assistant_interaction_subscriber.h"

namespace chromeos {
namespace assistant {

DefaultAssistantInteractionSubscriber::DefaultAssistantInteractionSubscriber() =
    default;
DefaultAssistantInteractionSubscriber::
    ~DefaultAssistantInteractionSubscriber() = default;

mojo::PendingRemote<mojom::AssistantInteractionSubscriber>
DefaultAssistantInteractionSubscriber::BindNewPipeAndPassRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

}  // namespace assistant
}  // namespace chromeos
