// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_WEB_VIEW_H_
#define ASH_ASSISTANT_UI_ASSISTANT_WEB_VIEW_H_

#include <map>
#include <memory>
#include <string>

#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/public/cpp/assistant/assistant_web_view_2.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/optional.h"
#include "ui/views/view.h"

namespace ash {

enum class AssistantButtonId;
class AssistantWebViewDelegate;

// TODO(b/146520500): Merge into AssistantWebContainerView.
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantWebView
    : public views::View,
      public AssistantWebView2::Observer {
 public:
  AssistantWebView(AssistantViewDelegate* assistant_view_delegate,
                   AssistantWebViewDelegate* web_container_view_delegate);
  ~AssistantWebView() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void ChildPreferredSizeChanged(views::View* child) override;

  // AssistantWebView2::Observer:
  void DidStopLoading() override;
  void DidSuppressNavigation(const GURL& url,
                             WindowOpenDisposition disposition,
                             bool from_user_gesture) override;
  void DidChangeCanGoBack(bool can_go_back) override;

  // Invoke to navigate back in the embedded WebContents' navigation stack. If
  // backwards navigation is not possible, returns |false|. Otherwise |true| to
  // indicate success.
  bool GoBack();

  // Invoke to open the specified |url|.
  void OpenUrl(const GURL& url);

 private:
  void InitLayout();
  void RemoveContents();

  // TODO(b/143177141): Remove AssistantViewDelegate.
  AssistantViewDelegate* const assistant_view_delegate_;
  AssistantWebViewDelegate* const web_container_view_delegate_;

  std::unique_ptr<AssistantWebView2> contents_view_;

  DISALLOW_COPY_AND_ASSIGN(AssistantWebView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_WEB_VIEW_H_
