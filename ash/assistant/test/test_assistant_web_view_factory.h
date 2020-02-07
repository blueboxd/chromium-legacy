// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_TEST_TEST_ASSISTANT_WEB_VIEW_FACTORY_H_
#define ASH_ASSISTANT_TEST_TEST_ASSISTANT_WEB_VIEW_FACTORY_H_

#include <memory>

#include "ash/public/cpp/assistant/assistant_web_view_factory.h"

namespace ash {

// An implementation of AssistantWebViewFactory for use in unittests.
class TestAssistantWebViewFactory : public AssistantWebViewFactory {
 public:
  TestAssistantWebViewFactory();
  TestAssistantWebViewFactory(const AssistantWebViewFactory& copy) = delete;
  TestAssistantWebViewFactory& operator=(
      const AssistantWebViewFactory& assign) = delete;
  ~TestAssistantWebViewFactory() override;

  // AssistantWebViewFactory:
  std::unique_ptr<AssistantWebView> Create(
      const AssistantWebView::InitParams& params) override;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_TEST_TEST_ASSISTANT_WEB_VIEW_FACTORY_H_
