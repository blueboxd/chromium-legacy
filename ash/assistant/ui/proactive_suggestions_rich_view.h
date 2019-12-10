// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_PROACTIVE_SUGGESTIONS_RICH_VIEW_H_
#define ASH_ASSISTANT_UI_PROACTIVE_SUGGESTIONS_RICH_VIEW_H_

#include <memory>

#include "ash/assistant/ui/proactive_suggestions_view.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/content/public/cpp/navigable_contents.h"
#include "services/content/public/cpp/navigable_contents_view.h"
#include "ui/events/event_observer.h"

namespace views {
class EventMonitor;
}  // namespace views

namespace ash {

class AssistantViewDelegate;
class ViewShadow;

// Rich entry point for the proactive suggestions feature.
class COMPONENT_EXPORT(ASSISTANT_UI) ProactiveSuggestionsRichView
    : public ProactiveSuggestionsView,
      public ui::EventObserver,
      public content::NavigableContentsObserver {
 public:
  explicit ProactiveSuggestionsRichView(AssistantViewDelegate* delegate);
  explicit ProactiveSuggestionsRichView(ProactiveSuggestionsRichView&) = delete;
  ProactiveSuggestionsRichView& operator=(ProactiveSuggestionsRichView&) =
      delete;
  ~ProactiveSuggestionsRichView() override;

  // ProactiveSuggestionsView:
  const char* GetClassName() const override;
  void InitLayout() override;
  void AddedToWidget() override;
  void ShowWhenReady() override;
  void Hide() override;
  void Close() override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // ui::EventObserver:
  using views::View::OnEvent;  // Suppress clang warning.
  void OnEvent(const ui::Event& event) override;

  // content::NavigableContentsObserver:
  void DidAutoResizeView(const gfx::Size& new_size) override;
  void DidStopLoading() override;
  void DidSuppressNavigation(const GURL& url,
                             WindowOpenDisposition disposition,
                             bool from_user_gesture) override;

 private:
  mojo::Remote<content::mojom::NavigableContentsFactory> contents_factory_;
  std::unique_ptr<content::NavigableContents> contents_;
  std::unique_ptr<views::EventMonitor> event_monitor_;
  std::unique_ptr<ViewShadow> view_shadow_;

  // Because the web contents that this view embeds loads asynchronously, it
  // may cause UI jank if we show our widget before loading has stopped. To
  // address this we won't show the widget for this view until our contents has
  // stopped loading and we use this flag to track if we have a pending show.
  bool show_when_ready_ = false;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_PROACTIVE_SUGGESTIONS_RICH_VIEW_H_
