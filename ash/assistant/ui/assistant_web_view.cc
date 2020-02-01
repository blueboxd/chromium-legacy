// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_web_view.h"

#include <algorithm>
#include <utility>

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_web_view_delegate.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/public/cpp/assistant/assistant_web_view_factory.h"
#include "base/bind.h"
#include "base/callback.h"
#include "ui/aura/window.h"
#include "ui/base/window_open_disposition.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

// AssistantWebView ------------------------------------------------------------

AssistantWebView::AssistantWebView(
    AssistantViewDelegate* assistant_view_delegate,
    AssistantWebViewDelegate* web_container_view_delegate)
    : assistant_view_delegate_(assistant_view_delegate),
      web_container_view_delegate_(web_container_view_delegate) {
  InitLayout();
}

AssistantWebView::~AssistantWebView() = default;

const char* AssistantWebView::GetClassName() const {
  return "AssistantWebView";
}

gfx::Size AssistantWebView::CalculatePreferredSize() const {
  return gfx::Size(kPreferredWidthDip, GetHeightForWidth(kPreferredWidthDip));
}

int AssistantWebView::GetHeightForWidth(int width) const {
  return INT_MAX;
}

void AssistantWebView::ChildPreferredSizeChanged(views::View* child) {
  // Because AssistantWebView has a fixed size, it does not re-layout its
  // children when their preferred size changes. To address this, we need to
  // explicitly request a layout pass.
  Layout();
  SchedulePaint();
}

void AssistantWebView::InitLayout() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
}

void AssistantWebView::DidStopLoading() {
  // We should only respond to the |DidStopLoading| event the first time, to add
  // the view for contents to our view hierarchy and perform other one-time view
  // initializations.
  if (contents_view_->parent())
    return;

  contents_view_->SetPreferredSize(GetPreferredSize());
  AddChildView(contents_view_.get());
  SetFocusBehavior(FocusBehavior::ALWAYS);
}

void AssistantWebView::DidSuppressNavigation(const GURL& url,
                                             WindowOpenDisposition disposition,
                                             bool from_user_gesture) {
  if (!from_user_gesture)
    return;

  // Deep links are always handled by AssistantViewDelegate. If the
  // |disposition| indicates a desire to open a new foreground tab, we also
  // defer to the AssistantViewDelegate so that it can open the |url| in the
  // browser.
  if (assistant::util::IsDeepLinkUrl(url) ||
      disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB) {
    assistant_view_delegate_->OpenUrlFromView(url);
    return;
  }

  // Otherwise we'll allow our WebContents to navigate freely.
  contents_view_->Navigate(url);
}

void AssistantWebView::DidChangeCanGoBack(bool can_go_back) {
  DCHECK(web_container_view_delegate_);
  web_container_view_delegate_->UpdateBackButtonVisibility(GetWidget(),
                                                           can_go_back);
}

bool AssistantWebView::GoBack() {
  return contents_view_ && contents_view_->GoBack();
}

void AssistantWebView::OpenUrl(const GURL& url) {
  RemoveContents();

  AssistantWebView2::InitParams contents_params;
  contents_params.suppress_navigation = true;

  contents_view_ = AssistantWebViewFactory::Get()->Create(contents_params);

  // We retain ownership of |contents_view_| as it is only added to the view
  // hierarchy once loading stops and we want to ensure that it is cleaned up in
  // the rare chance that that never occurs.
  contents_view_->set_owned_by_client();

  // We observe |contents_view_| so that we can handle events from the
  // underlying WebContents.
  contents_view_->AddObserver(this);

  // Navigate to the specified |url|.
  contents_view_->Navigate(url);
}

void AssistantWebView::RemoveContents() {
  if (!contents_view_)
    return;

  RemoveChildView(contents_view_.get());

  SetFocusBehavior(FocusBehavior::NEVER);

  contents_view_->RemoveObserver(this);
  contents_view_.reset();
}

}  // namespace ash
