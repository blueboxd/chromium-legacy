// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/proactive_suggestions_rich_view.h"

#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/public/cpp/assistant/proactive_suggestions.h"
#include "ash/public/cpp/view_shadow.h"
#include "base/base64.h"
#include "chromeos/services/assistant/public/features.h"
#include "ui/aura/window.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {

ProactiveSuggestionsRichView::ProactiveSuggestionsRichView(
    AssistantViewDelegate* delegate)
    : ProactiveSuggestionsView(delegate) {}

ProactiveSuggestionsRichView::~ProactiveSuggestionsRichView() {
  if (contents_)
    contents_->RemoveObserver(this);
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
      chromeos::assistant::features::
          GetProactiveSuggestionsRichEntryPointCornerRadius());

  // Initialize NavigableContentsFactory.
  delegate()->GetNavigableContentsFactoryForView(
      contents_factory_.BindNewPipeAndPassReceiver());

  // Initialize NavigableContentsParams.
  auto params = content::mojom::NavigableContentsParams::New();
  params->enable_view_auto_resize = true;
  params->auto_resize_min_size = gfx::Size(1, 1);
  params->auto_resize_max_size = gfx::Size(INT_MAX, INT_MAX);
  params->suppress_navigations = true;

  // Initialize NavigableContents.
  contents_ = std::make_unique<content::NavigableContents>(
      contents_factory_.get(), std::move(params));
  contents_->AddObserver(this);

  // Encode the html for the entry point to be URL safe.
  std::string encoded_html;
  base::Base64Encode(proactive_suggestions()->rich_entry_point_html(),
                     &encoded_html);

  // Navigate to the data URL representing our encoded HTML.
  constexpr char kDataUriPrefix[] = "data:text/html;base64,";
  contents_->Navigate(GURL(kDataUriPrefix + encoded_html));
}

void ProactiveSuggestionsRichView::AddedToWidget() {
  // Our embedded web contents will consume events that would otherwise reach
  // our view so we need to use an EventMonitor to still receive them.
  event_monitor_ = views::EventMonitor::CreateWindowMonitor(
      this, GetWidget()->GetNativeWindow(),
      {ui::ET_GESTURE_TAP, ui::ET_GESTURE_TAP_CANCEL, ui::ET_GESTURE_TAP_DOWN,
       ui::ET_MOUSE_ENTERED, ui::ET_MOUSE_MOVED, ui::ET_MOUSE_EXITED});
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

void ProactiveSuggestionsRichView::DidAutoResizeView(
    const gfx::Size& new_size) {
  contents_->GetView()->view()->SetPreferredSize(new_size);
  PreferredSizeChanged();
}

void ProactiveSuggestionsRichView::DidStopLoading() {
  AddChildView(contents_->GetView()->view());
  PreferredSizeChanged();

  // Once the view for the embedded web contents has been fully initialized,
  // it's safe to set our desired corner radius.
  contents_->GetView()->native_view()->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(
          chromeos::assistant::features::
              GetProactiveSuggestionsRichEntryPointCornerRadius()));

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
    delegate()->OpenUrlFromView(url);
}

}  // namespace ash
