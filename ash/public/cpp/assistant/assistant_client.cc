// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/assistant/assistant_client.h"

#include "base/check_op.h"

namespace ash {
namespace {

AssistantClient* g_assistant_client = nullptr;

}  // namespace

// static
AssistantClient* AssistantClient::Get() {
  return g_assistant_client;
}

AssistantClient::AssistantClient() {
  DCHECK(!g_assistant_client);
  g_assistant_client = this;
}

AssistantClient::~AssistantClient() {
  DCHECK_EQ(g_assistant_client, this);
  g_assistant_client = nullptr;
}

}  // namespace ash
