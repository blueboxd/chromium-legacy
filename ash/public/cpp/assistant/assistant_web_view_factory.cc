// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/assistant/assistant_web_view_factory.h"

#include "base/check_op.h"

namespace ash {

namespace {

AssistantWebViewFactory* g_instance = nullptr;

}  // namespace

AssistantWebViewFactory::AssistantWebViewFactory() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

AssistantWebViewFactory::~AssistantWebViewFactory() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
AssistantWebViewFactory* AssistantWebViewFactory::Get() {
  return g_instance;
}

}  // namespace ash
