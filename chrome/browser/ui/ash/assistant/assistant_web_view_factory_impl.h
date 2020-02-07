// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_WEB_VIEW_FACTORY_IMPL_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_WEB_VIEW_FACTORY_IMPL_H_

#include "ash/public/cpp/assistant/assistant_web_view_factory.h"

class Profile;

// Implements the singleton AssistantWebViewFactory used by Ash to work around
// dependency restrictions.
class AssistantWebViewFactoryImpl : public ash::AssistantWebViewFactory {
 public:
  explicit AssistantWebViewFactoryImpl(Profile* profile);
  ~AssistantWebViewFactoryImpl() override;

  AssistantWebViewFactoryImpl(AssistantWebViewFactoryImpl&) = delete;
  AssistantWebViewFactoryImpl& operator=(AssistantWebViewFactoryImpl&) = delete;

  // ash::AssistantWebViewFactory:
  std::unique_ptr<ash::AssistantWebView> Create(
      const ash::AssistantWebView::InitParams& params) override;

 private:
  Profile* const profile_;
};

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_WEB_VIEW_FACTORY_IMPL_H_
