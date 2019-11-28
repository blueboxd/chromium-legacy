// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_ERROR_BUBBLE_H_
#define ASH_LOGIN_UI_LOGIN_ERROR_BUBBLE_H_

#include "ash/ash_export.h"
#include "ash/login/ui/login_base_bubble_view.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/view.h"

namespace ash {

class ASH_EXPORT LoginErrorBubble : public LoginBaseBubbleView {
 public:
  LoginErrorBubble();
  LoginErrorBubble(views::View* content,
                   views::View* anchor_view,
                   bool is_persistent);
  ~LoginErrorBubble() override;

  void SetContent(views::View* content);
  // We set an accessible name when content is not accessible. This happens if
  // content is a container (e.g. a text and a "learn more" button). In such a
  // case, it will have multiple subviews but only one which needs to be read
  // on bubble show – when the alert event occurs.
  void SetAccessibleName(const base::string16& name);

  // LoginBaseBubbleView:
  bool IsPersistent() const override;
  void SetPersistent(bool persistent) override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  const char* GetClassName() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  views::View* content_ = nullptr;
  bool is_persistent_;

  // Accessibility data.
  base::string16 accessible_name_;

  DISALLOW_COPY_AND_ASSIGN(LoginErrorBubble);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_ERROR_BUBBLE_H_
