// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/assistant/conversation_starters_client.h"

#include "base/check_op.h"

namespace ash {

namespace {

// The singleton instance implemented in the browser.
ConversationStartersClient* g_instance = nullptr;

}  // namespace

ConversationStartersClient::ConversationStartersClient() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

ConversationStartersClient::~ConversationStartersClient() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
ConversationStartersClient* ConversationStartersClient::Get() {
  return g_instance;
}

}  // namespace ash
