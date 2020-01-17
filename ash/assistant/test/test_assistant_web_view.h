// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_TEST_TEST_ASSISTANT_WEB_VIEW_H_
#define ASH_ASSISTANT_TEST_TEST_ASSISTANT_WEB_VIEW_H_

#include "ash/public/cpp/assistant/assistant_web_view_2.h"
#include "base/observer_list.h"

namespace ash {

// An implementation of AssistantWebView2 for use in unittests.
class TestAssistantWebView : public AssistantWebView2 {
 public:
  TestAssistantWebView();
  TestAssistantWebView(const TestAssistantWebView& copy) = delete;
  TestAssistantWebView& operator=(const TestAssistantWebView& assign) = delete;
  ~TestAssistantWebView() override;

  // AssistantWebView2:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  gfx::NativeView GetNativeView() override;
  bool GoBack() override;
  void Navigate(const GURL& url) override;

 private:
  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<TestAssistantWebView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_ASSISTANT_TEST_TEST_ASSISTANT_WEB_VIEW_H_
