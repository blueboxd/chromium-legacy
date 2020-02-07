// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/test/test_assistant_web_view_factory.h"

#include "ash/assistant/test/test_assistant_web_view.h"

namespace ash {

TestAssistantWebViewFactory::TestAssistantWebViewFactory() = default;

TestAssistantWebViewFactory::~TestAssistantWebViewFactory() = default;

std::unique_ptr<AssistantWebView> TestAssistantWebViewFactory::Create(
    const AssistantWebView::InitParams& params) {
  return std::make_unique<TestAssistantWebView>();
}

}  // namespace ash