// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_CLIENT_H_
#define ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_CLIENT_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/callback_forward.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/accessibility/mojom/ax_assistant_structure.mojom.h"

namespace ash {

// Interface for an Assistant client in Browser process.
class ASH_PUBLIC_EXPORT AssistantClient {
 public:
  using RequestAssistantStructureCallback =
      base::OnceCallback<void(ax::mojom::AssistantExtraPtr,
                              std::unique_ptr<ui::AssistantTree>)>;

  static AssistantClient* Get();

  // Binds the main assistant backend interface.
  virtual void BindAssistant(
      mojo::PendingReceiver<chromeos::assistant::mojom::Assistant>
          receiver) = 0;

  // Requests Assistant structure for the active browser or ARC++ app window.
  virtual void RequestAssistantStructure(
      RequestAssistantStructureCallback callback) = 0;

 protected:
  AssistantClient();
  virtual ~AssistantClient();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_CLIENT_H_
