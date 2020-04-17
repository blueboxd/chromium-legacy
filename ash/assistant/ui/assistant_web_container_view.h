// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_WEB_CONTAINER_VIEW_H_
#define ASH_ASSISTANT_UI_ASSISTANT_WEB_CONTAINER_VIEW_H_

#include "ash/public/cpp/assistant/assistant_web_view.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

class AssistantWebViewDelegate;

// The container for hosting standalone WebContents in Assistant.
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantWebContainerView
    : public views::WidgetDelegateView,
      public AssistantWebView::Observer {
 public:
  explicit AssistantWebContainerView(
      AssistantWebViewDelegate* web_container_view_delegate);
  ~AssistantWebContainerView() override;

  // views::WidgetDelegateView:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void ChildPreferredSizeChanged(views::View* child) override;

  // AssistantWebView::Observer:
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

  AssistantWebViewDelegate* const web_container_view_delegate_;

  std::unique_ptr<AssistantWebView> contents_view_;

  DISALLOW_COPY_AND_ASSIGN(AssistantWebContainerView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_WEB_CONTAINER_VIEW_H_
