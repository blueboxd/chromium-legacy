// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_web_container_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_web_view_delegate.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/public/cpp/assistant/assistant_web_view_factory.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/background.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/window/caption_button_layout_constants.h"

namespace ash {

namespace {

// This height includes the window's |non_client_frame_view|'s height.
constexpr int kPreferredWindowHeightDip = 768;
constexpr int kPreferredWindowWidthDip = 768;

// The minimum margin of the window to the edges of the screen.
constexpr int kMinWindowMarginDip = 48;

}  // namespace

AssistantWebContainerView::AssistantWebContainerView(
    AssistantWebViewDelegate* web_container_view_delegate)
    : web_container_view_delegate_(web_container_view_delegate) {
  InitLayout();
}

AssistantWebContainerView::~AssistantWebContainerView() = default;

const char* AssistantWebContainerView::GetClassName() const {
  return "AssistantWebContainerView";
}

gfx::Size AssistantWebContainerView::CalculatePreferredSize() const {
  const int non_client_frame_view_height =
      views::GetCaptionButtonLayoutSize(
          views::CaptionButtonLayoutSize::kNonBrowserCaption)
          .height();

  const gfx::Rect work_area =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(GetWidget()->GetNativeWindow())
          .work_area();

  const int width = std::min(work_area.width() - 2 * kMinWindowMarginDip,
                             kPreferredWindowWidthDip);

  const int height = std::min(work_area.height() - 2 * kMinWindowMarginDip,
                              kPreferredWindowHeightDip) -
                     non_client_frame_view_height;

  return gfx::Size(width, height);
}

void AssistantWebContainerView::ChildPreferredSizeChanged(views::View* child) {
  // Because AssistantWebContainerView has a fixed size, it does not re-layout
  // its children when their preferred size changes. To address this, we need to
  // explicitly request a layout pass.
  Layout();
  SchedulePaint();
}

void AssistantWebContainerView::DidStopLoading() {
  // We should only respond to the |DidStopLoading| event the first time, to add
  // the view for contents to our view hierarchy and perform other one-time view
  // initializations.
  if (contents_view_->parent())
    return;

  contents_view_->SetPreferredSize(GetPreferredSize());
  AddChildView(contents_view_.get());
  SetFocusBehavior(FocusBehavior::ALWAYS);
}

void AssistantWebContainerView::DidSuppressNavigation(
    const GURL& url,
    WindowOpenDisposition disposition,
    bool from_user_gesture) {
  if (!from_user_gesture)
    return;

  // Deep links are always handled by the AssistantViewDelegate. If the
  // |disposition| indicates a desire to open a new foreground tab, we also
  // defer to the AssistantViewDelegate so that it can open the |url| in the
  // browser.
  if (assistant::util::IsDeepLinkUrl(url) ||
      disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB) {
    AssistantController::Get()->OpenUrl(url);
    return;
  }

  // Otherwise we'll allow our WebContents to navigate freely.
  contents_view_->Navigate(url);
}

void AssistantWebContainerView::DidChangeCanGoBack(bool can_go_back) {
  DCHECK(web_container_view_delegate_);
  web_container_view_delegate_->UpdateBackButtonVisibility(GetWidget(),
                                                           can_go_back);
}

bool AssistantWebContainerView::GoBack() {
  return contents_view_ && contents_view_->GoBack();
}

void AssistantWebContainerView::OpenUrl(const GURL& url) {
  RemoveContents();

  AssistantWebView::InitParams contents_params;
  contents_params.suppress_navigation = true;
  contents_params.minimize_on_back_key = true;

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

void AssistantWebContainerView::InitLayout() {
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW;
  params.delegate = this;
  params.name = GetClassName();

  views::Widget* widget = new views::Widget;
  widget->Init(std::move(params));

  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
}

void AssistantWebContainerView::RemoveContents() {
  if (!contents_view_)
    return;

  RemoveChildView(contents_view_.get());

  SetFocusBehavior(FocusBehavior::NEVER);

  contents_view_->RemoveObserver(this);
  contents_view_.reset();
}

}  // namespace ash
