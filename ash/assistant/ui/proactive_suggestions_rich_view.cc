// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/proactive_suggestions_rich_view.h"

#include <string>

#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/public/cpp/assistant/assistant_web_view_factory.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/proactive_suggestions.h"
#include "ash/public/cpp/view_shadow.h"
#include "base/base64.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "ui/aura/window.h"
#include "ui/views/background.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {

namespace {

using chromeos::assistant::features::
    GetProactiveSuggestionsRichEntryPointBackgroundBlurRadius;
using chromeos::assistant::features::
    GetProactiveSuggestionsRichEntryPointCornerRadius;

}  // namespace

// ProactiveSuggestionsRichView ------------------------------------------------

ProactiveSuggestionsRichView::ProactiveSuggestionsRichView(
    AssistantViewDelegate* delegate)
    : ProactiveSuggestionsView(delegate) {}

ProactiveSuggestionsRichView::~ProactiveSuggestionsRichView() {
  if (contents_view_)
    contents_view_->RemoveObserver(this);
}

const char* ProactiveSuggestionsRichView::GetClassName() const {
  return "ProactiveSuggestionsRichView";
}

void ProactiveSuggestionsRichView::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Initialize shadow.
  view_shadow_ =
      std::make_unique<ViewShadow>(this, wm::kShadowElevationActiveWindow);
  view_shadow_->SetRoundedCornerRadius(
      GetProactiveSuggestionsRichEntryPointCornerRadius());

  // Initialize |contents_view_| params.
  AssistantWebView::InitParams params;
  params.enable_auto_resize = true;
  params.min_size = gfx::Size(1, 1);
  params.max_size = gfx::Size(INT_MAX, INT_MAX);
  params.suppress_navigation = true;

  // Initialize |contents_view_|.
  // Note that we retain ownership of the underlying pointer so that it is
  // cleaned up in the event that the view is never added to the view hierarchy.
  contents_view_ = AssistantWebViewFactory::Get()->Create(params);
  contents_view_->set_owned_by_client();
  contents_view_->AddObserver(this);

  // Encode the html for the entry point to be URL safe.
  std::string encoded_html;
  base::Base64Encode(proactive_suggestions()->rich_entry_point_html(),
                     &encoded_html);

  // Navigate to the data URL representing our encoded HTML.
  constexpr char kDataUriPrefix[] = "data:text/html;base64,";
  contents_view_->Navigate(GURL(kDataUriPrefix + encoded_html));
}

void ProactiveSuggestionsRichView::AddedToWidget() {
  // Our embedded web contents will consume events that would otherwise reach
  // our view so we need to use an EventMonitor to still receive them.
  event_monitor_ = views::EventMonitor::CreateWindowMonitor(
      this, GetWidget()->GetNativeWindow(),
      {ui::ET_GESTURE_TAP, ui::ET_GESTURE_TAP_CANCEL, ui::ET_GESTURE_TAP_DOWN,
       ui::ET_MOUSE_ENTERED, ui::ET_MOUSE_EXITED});
}

void ProactiveSuggestionsRichView::ChildPreferredSizeChanged(
    views::View* child) {
  PreferredSizeChanged();
}

void ProactiveSuggestionsRichView::OnMouseEntered(const ui::MouseEvent& event) {
  // Our embedded web contents is expected to consume events that would
  // otherwise reach our view. We instead handle these events in OnEvent().
  NOTREACHED();
}

void ProactiveSuggestionsRichView::OnMouseExited(const ui::MouseEvent& event) {
  // Our embedded web contents is expected to consume events that would
  // otherwise reach our view. We instead handle these events in OnEvent().
  NOTREACHED();
}

void ProactiveSuggestionsRichView::OnGestureEvent(ui::GestureEvent* event) {
  // Our embedded web contents is expected to consume events that would
  // otherwise reach our view. We instead handle these events in OnEvent().
  NOTREACHED();
}

void ProactiveSuggestionsRichView::OnEvent(const ui::Event& event) {
  switch (event.type()) {
    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_TAP_CANCEL:
    case ui::ET_MOUSE_EXITED:
      delegate()->OnProactiveSuggestionsViewHoverChanged(/*is_hovering=*/false);
      break;
    case ui::ET_GESTURE_TAP_DOWN:
    case ui::ET_MOUSE_ENTERED:
      delegate()->OnProactiveSuggestionsViewHoverChanged(/*is_hovering=*/true);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void ProactiveSuggestionsRichView::ShowWhenReady() {
  // If no children have yet been added to the layout, the embedded web contents
  // has not yet finished loading. In this case, we'll set a flag and delay
  // showing until loading stops to avoid UI jank.
  if (children().empty()) {
    show_when_ready_ = true;
    return;
  }
  // Otherwise it's fine to go ahead w/ showing the widget immediately.
  GetWidget()->ShowInactive();
  show_when_ready_ = false;
}

void ProactiveSuggestionsRichView::Hide() {
  show_when_ready_ = false;
  ProactiveSuggestionsView::Hide();
}

void ProactiveSuggestionsRichView::Close() {
  show_when_ready_ = false;
  ProactiveSuggestionsView::Close();
}

void ProactiveSuggestionsRichView::DidStopLoading() {
  AddChildView(contents_view_.get());
  PreferredSizeChanged();

  // Once the view for the embedded web contents has been fully initialized,
  // it's safe to set our desired corner radius.
  contents_view_->GetNativeView()->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(
          GetProactiveSuggestionsRichEntryPointCornerRadius()));

  // If specified, we also blur background to increase readability of this view
  // which semi-transparently sits atop other content. This also serves to help
  // us blend more with other system UI (e.g. quick settings).
  const int background_blur_radius =
      GetProactiveSuggestionsRichEntryPointBackgroundBlurRadius();
  if (background_blur_radius > 0) {
    contents_view_->GetNativeView()->layer()->SetBackgroundBlur(
        background_blur_radius);
  }

  // This view should now be fully initialized, so it's safe to show without
  // risk of introducing UI jank if we so desire.
  if (show_when_ready_)
    ShowWhenReady();
}

void ProactiveSuggestionsRichView::DidSuppressNavigation(
    const GURL& url,
    WindowOpenDisposition disposition,
    bool from_user_gesture) {
  if (from_user_gesture)
    AssistantController::Get()->OpenUrl(url);
}

}  // namespace ash
