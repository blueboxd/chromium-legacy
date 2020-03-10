// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_TEST_TEST_ASSISTANT_CLIENT_H_
#define ASH_ASSISTANT_TEST_TEST_ASSISTANT_CLIENT_H_

#include "ash/public/cpp/assistant/assistant_client.h"

namespace ash {

// Dummy implementation of the Assistant client.
class TestAssistantClient : public AssistantClient {
 public:
  TestAssistantClient();
  TestAssistantClient(const TestAssistantClient&) = delete;
  TestAssistantClient& operator=(TestAssistantClient&) = delete;
  ~TestAssistantClient() override;

  // AssistantClient:
  void BindAssistant(
      mojo::PendingReceiver<chromeos::assistant::mojom::Assistant> receiver)
      override {}
  void RequestAssistantStructure(
      RequestAssistantStructureCallback callback) override;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_TEST_TEST_ASSISTANT_CLIENT_H_
