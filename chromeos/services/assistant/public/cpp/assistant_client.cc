// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/public/cpp/assistant_client.h"

namespace chromeos {
namespace assistant {

namespace {

AssistantClient* g_instance = nullptr;

}  // namespace

// static
AssistantClient* AssistantClient::Get() {
  DCHECK(g_instance);
  return g_instance;
}

AssistantClient::AssistantClient() {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

AssistantClient::~AssistantClient() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace assistant
}  // namespace chromeos
