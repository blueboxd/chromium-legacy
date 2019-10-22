// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_WEB_CONTAINER_VIEW_H_
#define ASH_ASSISTANT_UI_ASSISTANT_WEB_CONTAINER_VIEW_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

class AssistantViewDelegate;

// The container of assistant_web_view when Assistant web container is enabled.
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantWebContainerView
    : public views::WidgetDelegateView {
 public:
  explicit AssistantWebContainerView(AssistantViewDelegate* delegate);
  ~AssistantWebContainerView() override;

  // views::WidgetDelegateView:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;

 private:
  void InitLayout();

  AssistantViewDelegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(AssistantWebContainerView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_WEB_CONTAINER_VIEW_H_
