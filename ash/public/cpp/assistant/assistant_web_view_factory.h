// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_WEB_VIEW_FACTORY_H_
#define ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_WEB_VIEW_FACTORY_H_

#include <memory>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/assistant/assistant_web_view.h"

namespace ash {

// A factory implemented in Browser which is responsible for creating instances
// of AssistantWebView to work around dependency restrictions in Ash.
class ASH_PUBLIC_EXPORT AssistantWebViewFactory {
 public:
  // Returns the singleton factory instance.
  static AssistantWebViewFactory* Get();

  // Creates a new AssistantWebView instance with the given |params|.
  virtual std::unique_ptr<AssistantWebView> Create(
      const AssistantWebView::InitParams& params) = 0;

 protected:
  AssistantWebViewFactory();
  virtual ~AssistantWebViewFactory();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_WEB_VIEW_FACTORY_H_
