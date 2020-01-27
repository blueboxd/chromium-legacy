// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_CLIENT_H_
#define ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_CLIENT_H_

#include "ash/public/cpp/ash_public_export.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash {

// Interface for an Assistant client in Browser process.
class ASH_PUBLIC_EXPORT AssistantClient {
 public:
  static AssistantClient* Get();

  virtual void BindAssistant(
      mojo::PendingReceiver<chromeos::assistant::mojom::Assistant>
          receiver) = 0;

 protected:
  AssistantClient();
  virtual ~AssistantClient();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_CLIENT_H_
