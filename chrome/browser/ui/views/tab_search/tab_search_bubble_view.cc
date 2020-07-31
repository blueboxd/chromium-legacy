// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_search/tab_search_bubble_view.h"

#include "base/metrics/histogram_functions.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"
#endif

namespace {

// The min / max size available to the TabSearchBubbleView.
// These are arbitrary sizes that match those set by ExtensionPopup.
// TODO(tluk): Determine the correct size constraints for the
// TabSearchBubbleView.
constexpr gfx::Size kMinSize(25, 25);
constexpr gfx::Size kMaxSize(800, 600);

class TabSearchWebView : public views::WebView {
 public:
  TabSearchWebView(content::BrowserContext* browser_context,
                   TabSearchBubbleView* parent)
      : WebView(browser_context), parent_(parent) {}

  ~TabSearchWebView() override {
    if (timer_.has_value()) {
      UmaHistogramMediumTimes("Tabs.TabSearch.WindowDisplayedDuration",
                              timer_->Elapsed());
    }
  }

  // views::WebView:
  void PreferredSizeChanged() override {
    WebView::PreferredSizeChanged();
    parent_->OnWebViewSizeChanged();
  }

  void OnWebContentsAttached() override { SetVisible(false); }

  void ResizeDueToAutoResize(content::WebContents* web_contents,
                             const gfx::Size& new_size) override {
    // Don't actually do anything with this information until we have been
    // shown. Size changes will not be honored by lower layers while we are
    // hidden.
    if (!GetVisible()) {
      pending_preferred_size_ = new_size;
      return;
    }
    WebView::ResizeDueToAutoResize(web_contents, new_size);
  }

  void DocumentOnLoadCompletedInMainFrame() override {
    GetWidget()->Show();
    GetWebContents()->Focus();

    // Track window open times from when the bubble is first shown.
    timer_ = base::ElapsedTimer();
  }

  void DidStopLoading() override {
    if (GetVisible())
      return;

    SetVisible(true);
    ResizeDueToAutoResize(web_contents(), pending_preferred_size_);
  }

 private:
  TabSearchBubbleView* parent_;

  // What we should set the preferred width to once TabSearch has loaded.
  gfx::Size pending_preferred_size_;

  // Time the Tab Search window has been open.
  base::Optional<base::ElapsedTimer> timer_;
};

}  // namespace

#if defined(USE_AURA)
class TabSearchBubbleView::TabSearchWindowObserverAura
    : public wm::ActivationChangeObserver {
 public:
  explicit TabSearchWindowObserverAura(TabSearchBubbleView* bubble)
      : bubble_(bubble) {
    gfx::NativeView native_view = bubble_->GetWidget()->GetNativeView();
    // This is removed in the destructor called by
    // TabSearchBubbleView::OnWidgetDestroying(), which is guaranteed to be
    // called before the Widget goes away.  It's not safe to use a
    // ScopedObserver for this, since the activation client may be deleted
    // without a call back to this class.
    wm::GetActivationClient(native_view->GetRootWindow())->AddObserver(this);
  }

  ~TabSearchWindowObserverAura() override {
    auto* activation_client = wm::GetActivationClient(
        bubble_->GetWidget()->GetNativeWindow()->GetRootWindow());
    DCHECK(activation_client);
    activation_client->RemoveObserver(this);
  }

  // wm::ActivationChangeObserver:
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override {
    // Close on anchor window activation (i.e. user clicked the browser window).
    // DesktopNativeWidgetAura does not trigger the expected browser widget
    // [de]activation events when activating widgets in its own root window.
    // This additional check handles those cases. See https://crbug.com/320889 .
    views::Widget* anchor_widget = bubble_->anchor_widget();
    if (anchor_widget && gained_active == anchor_widget->GetNativeWindow()) {
      bubble_->GetWidget()->CloseWithReason(
          views::Widget::ClosedReason::kLostFocus);
    }
  }

 private:
  TabSearchBubbleView* bubble_;
};
#endif

void TabSearchBubbleView::CreateTabSearchBubble(
    content::BrowserContext* browser_context,
    views::View* anchor_view) {
  auto delegate =
      std::make_unique<TabSearchBubbleView>(browser_context, anchor_view);
  BubbleDialogDelegateView::CreateBubble(delegate.release());
}

TabSearchBubbleView::TabSearchBubbleView(
    content::BrowserContext* browser_context,
    views::View* anchor_view)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      web_view_(AddChildView(
          std::make_unique<TabSearchWebView>(browser_context, this))) {
  DCHECK(anchor_view);
  observed_anchor_widget_.Add(anchor_view->GetWidget());

  set_close_on_deactivate(false);

  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_margins(gfx::Insets());

  SetLayoutManager(std::make_unique<views::FillLayout>());
  web_view_->EnableSizingFromWebContents(kMinSize, kMaxSize);
  web_view_->LoadInitialURL(GURL(chrome::kChromeUITabSearchURL));
}

TabSearchBubbleView::~TabSearchBubbleView() = default;

gfx::Size TabSearchBubbleView::CalculatePreferredSize() const {
  // Constrain the size to popup min/max.
  gfx::Size preferred_size = views::View::CalculatePreferredSize();
  preferred_size.SetToMax(kMinSize);
  preferred_size.SetToMin(kMaxSize);
  return preferred_size;
}

void TabSearchBubbleView::AddedToWidget() {
  BubbleDialogDelegateView::AddedToWidget();
  observed_bubble_widget_.Add(GetWidget());
#if defined(USE_AURA)
  // |window_observer_| deals with activation issues relevant to Aura platforms.
  // This special case handling is not needed on Mac.
  window_observer_ = std::make_unique<TabSearchWindowObserverAura>(this);
#endif
}

void TabSearchBubbleView::OnWidgetActivationChanged(views::Widget* widget,
                                                    bool active) {
  // The widget is shown asynchronously and may take a long time to appear, so
  // only close if it's actually been shown.
  if (GetWidget()->IsVisible() && widget == anchor_widget() && active)
    GetWidget()->CloseWithReason(views::Widget::ClosedReason::kLostFocus);
}

void TabSearchBubbleView::OnWidgetDestroying(views::Widget* widget) {
#if defined(USE_AURA)
  if (widget == GetWidget())
    window_observer_.reset();
#endif
}

void TabSearchBubbleView::OnWebViewSizeChanged() {
  SizeToContents();
}
