// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/webui_tab_strip_container_view.h"

#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/message_formatter.h"
#include "base/i18n/number_formatting.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_view_class_properties.h"
#include "chrome/browser/ui/views/feature_promos/feature_promo_bubble_view.h"
#include "chrome/browser/ui/views/feature_promos/feature_promo_colors.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/webui_tab_counter_button.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_layout.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_metrics.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "content/public/common/drop_data.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

namespace {

bool EventTypeCanCloseTabStrip(const ui::EventType& type) {
  switch (type) {
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_TOUCH_PRESSED:
    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_DOUBLE_TAP:
      return true;
    default:
      return false;
  }
}

class WebUITabStripWebView : public views::WebView {
 public:
  explicit WebUITabStripWebView(content::BrowserContext* context)
      : views::WebView(context) {}

  // content::WebContentsDelegate:
  bool CanDragEnter(content::WebContents* source,
                    const content::DropData& data,
                    blink::WebDragOperationsMask operations_allowed) override {
    // TODO(crbug.com/1032592): Prevent dragging across Chromium instances.
    if (data.custom_data.find(base::ASCIIToUTF16(kWebUITabIdDataType)) !=
        data.custom_data.end()) {
      int tab_id;
      bool found_tab_id = base::StringToInt(
          data.custom_data.at(base::ASCIIToUTF16(kWebUITabIdDataType)),
          &tab_id);
      return found_tab_id && extensions::ExtensionTabUtil::GetTabById(
                                 tab_id, browser_context(), false, nullptr);
    }

    if (data.custom_data.find(base::ASCIIToUTF16(kWebUITabGroupIdDataType)) !=
        data.custom_data.end()) {
      std::string group_id = base::UTF16ToUTF8(
          data.custom_data.at(base::ASCIIToUTF16(kWebUITabGroupIdDataType)));
      Browser* found_browser = tab_strip_ui::GetBrowserWithGroupId(
          Profile::FromBrowserContext(browser_context()), group_id);
      return found_browser != nullptr;
    }

    return false;
  }
};

}  // namespace

// When enabled, closes the container upon any event in the window not
// destined for the container and cancels the event. If an event is
// destined for the container, it passes it through.
class WebUITabStripContainerView::AutoCloser : public ui::EventHandler {
 public:
  using EventPassthroughPredicate =
      base::RepeatingCallback<bool(const ui::Event& event)>;

  AutoCloser(EventPassthroughPredicate event_passthrough_predicate,
             base::RepeatingClosure close_container_callback)
      : event_passthrough_predicate_(std::move(event_passthrough_predicate)),
        close_container_callback_(std::move(close_container_callback)) {}

  ~AutoCloser() override {}

  // Sets whether to inspect events. If not enabled, all events are
  // ignored and passed through as usual.
  void set_enabled(bool enabled) { enabled_ = enabled; }

  // ui::EventHandler:
  void OnEvent(ui::Event* event) override {
    if (!enabled_)
      return;
    if (event_passthrough_predicate_.Run(*event))
      return;

    event->StopPropagation();
    close_container_callback_.Run();
  }

 private:
  EventPassthroughPredicate event_passthrough_predicate_;
  base::RepeatingClosure close_container_callback_;
  bool enabled_ = false;
};

WebUITabStripContainerView::WebUITabStripContainerView(
    Browser* browser,
    views::View* tab_contents_container)
    : browser_(browser),
      web_view_(AddChildView(
          std::make_unique<WebUITabStripWebView>(browser->profile()))),
      tab_contents_container_(tab_contents_container),
      iph_tracker_(feature_engagement::TrackerFactory::GetForBrowserContext(
          browser_->profile())),
      auto_closer_(std::make_unique<AutoCloser>(
          base::Bind(&WebUITabStripContainerView::EventShouldPropagate,
                     base::Unretained(this)),
          base::Bind(&WebUITabStripContainerView::CloseForEventOutsideTabStrip,
                     base::Unretained(this)))) {
  DCHECK(UseTouchableTabStrip());
  animation_.SetTweenType(gfx::Tween::Type::FAST_OUT_SLOW_IN);

  // Our observed Widget's NativeView may be destroyed before us. We
  // have no reasonable way of un-registering our pre-target handler
  // from the NativeView while the Widget is destroying. This disables
  // EventHandler's check that it has been removed from all
  // EventTargets.
  auto_closer_->DisableCheckTargets();

  SetVisible(false);
  animation_.Reset(0.0);

  // TODO(crbug.com/1010589) WebContents are initially assumed to be visible by
  // default unless explicitly hidden. The WebContents need to be set to hidden
  // so that the visibility state of the document in JavaScript is correctly
  // initially set to 'hidden', and the 'visibilitychange' events correctly get
  // fired.
  web_view_->GetWebContents()->WasHidden();

  web_view_->set_allow_accelerators(true);

  SetLayoutManager(std::make_unique<views::FillLayout>());
  web_view_->LoadInitialURL(GURL(chrome::kChromeUITabStripURL));
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_view_->web_contents());
  task_manager::WebContentsTags::CreateForTabContents(
      web_view_->web_contents());

  DCHECK(tab_contents_container);
  view_observer_.Add(tab_contents_container_);
  desired_height_ = TabStripUILayout::CalculateForWebViewportSize(
                        tab_contents_container_->size())
                        .CalculateContainerHeight();

  TabStripUI* const tab_strip_ui = static_cast<TabStripUI*>(
      web_view_->GetWebContents()->GetWebUI()->GetController());
  tab_strip_ui->Initialize(browser_, this);
}

WebUITabStripContainerView::~WebUITabStripContainerView() {
  // The NewTabButton and TabCounter button both use |this| as a listener. We
  // need to make sure we outlive them.
  delete new_tab_button_;
  delete tab_counter_;
}

bool WebUITabStripContainerView::UseTouchableTabStrip() {
  return base::FeatureList::IsEnabled(features::kWebUITabStrip) &&
         ui::MaterialDesignController::touch_ui();
}

views::NativeViewHost* WebUITabStripContainerView::GetNativeViewHost() {
  return web_view_->holder();
}

std::unique_ptr<ToolbarButton>
WebUITabStripContainerView::CreateNewTabButton() {
  DCHECK_EQ(nullptr, new_tab_button_);
  auto new_tab_button = std::make_unique<ToolbarButton>(this);
  new_tab_button->SetID(VIEW_ID_WEBUI_TAB_STRIP_NEW_TAB_BUTTON);
  new_tab_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_NEW_TAB));

  const int button_height = GetLayoutConstant(TOOLBAR_BUTTON_HEIGHT);
  new_tab_button->SetPreferredSize(gfx::Size(button_height, button_height));
  new_tab_button->SetHorizontalAlignment(gfx::ALIGN_CENTER);

  new_tab_button_ = new_tab_button.get();
  view_observer_.Add(new_tab_button_);

  return new_tab_button;
}

std::unique_ptr<views::View> WebUITabStripContainerView::CreateTabCounter() {
  DCHECK_EQ(nullptr, tab_counter_);

  auto tab_counter =
      CreateWebUITabCounterButton(this, browser_->tab_strip_model());

  tab_counter_ = tab_counter.get();
  view_observer_.Add(tab_counter_);

  return tab_counter;
}

void WebUITabStripContainerView::UpdateButtons() {
  const SkColor normal_color =
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON);
  if (new_tab_button_) {
    new_tab_button_->SetImage(
        views::Button::STATE_NORMAL,
        gfx::CreateVectorIcon(kNewTabToolbarButtonIcon, normal_color));
  }
}

void WebUITabStripContainerView::UpdatePromoBubbleBounds() {
  if (!tab_counter_promo_)
    return;
  tab_counter_promo_->OnAnchorBoundsChanged();
}

void WebUITabStripContainerView::SetVisibleForTesting(bool visible) {
  SetContainerTargetVisibility(visible);
  animation_.SetCurrentValue(visible ? 1.0 : 0.0);
  animation_.End();
  PreferredSizeChanged();
}

const ui::AcceleratorProvider*
WebUITabStripContainerView::GetAcceleratorProvider() const {
  return BrowserView::GetBrowserViewForBrowser(browser_);
}

void WebUITabStripContainerView::CloseContainer() {
  SetContainerTargetVisibility(false);
  iph_tracker_->NotifyEvent(feature_engagement::events::kWebUITabStripClosed);
}

void WebUITabStripContainerView::SetContainerTargetVisibility(
    bool target_visible) {
  if (target_visible) {
    SetVisible(true);
    animation_.SetSlideDuration(base::TimeDelta::FromMilliseconds(250));
    animation_.Show();
    web_view_->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
    time_at_open_ = base::TimeTicks::Now();

    // If we're opening, end IPH if it's showing.
    if (tab_counter_promo_) {
      widget_observer_.Remove(tab_counter_promo_->GetWidget());
      tab_counter_promo_->GetWidget()->CloseWithReason(
          views::Widget::ClosedReason::kUnspecified);
      tab_counter_promo_ = nullptr;
      tab_counter_->SetProperty(kHasInProductHelpPromoKey, false);
      iph_tracker_->Dismissed(feature_engagement::kIPHWebUITabStripFeature);
    }
  } else {
    if (time_at_open_) {
      RecordTabStripUIOpenDurationHistogram(base::TimeTicks::Now() -
                                            time_at_open_.value());
      time_at_open_ = base::nullopt;
    }

    animation_.SetSlideDuration(base::TimeDelta::FromMilliseconds(200));
    animation_.Hide();
    web_view_->SetFocusBehavior(FocusBehavior::NEVER);

    // Tapping in the WebUI tab strip gives keyboard focus to the
    // WebContents's native window. While this doesn't take away View
    // focus, it will change the focused TextInputClient; see
    // |ui::InputMethod::SetFocusedTextInputClient()|. The Omnibox is a
    // TextInputClient, and it installs itself as the focused
    // TextInputClient when it receives Views-focus. So, tapping in the
    // tab strip while the Omnibox has focus will mean text cannot be
    // entered until it is blurred and re-focused. This caused
    // crbug.com/1027375.
    //
    // TODO(crbug.com/994350): stop WebUI tab strip from taking focus on
    // tap and remove this workaround.
    views::FocusManager* const focus_manager = GetFocusManager();
    if (focus_manager) {
      focus_manager->StoreFocusedView(true /* clear_native_focus */);
      focus_manager->RestoreFocusedView();
    }
  }
  auto_closer_->set_enabled(target_visible);
}

bool WebUITabStripContainerView::EventShouldPropagate(const ui::Event& event) {
  if (!event.IsLocatedEvent())
    return true;
  const ui::LocatedEvent* located_event = event.AsLocatedEvent();

  if (!EventTypeCanCloseTabStrip(located_event->type()))
    return true;

  // If the event is in the container or control buttons, let it be handled.
  for (views::View* view :
       {static_cast<views::View*>(this),
        static_cast<views::View*>(new_tab_button_), tab_counter_}) {
    if (!view)
      continue;

    const gfx::Rect bounds_in_screen = view->GetBoundsInScreen();
    const gfx::Point event_location_in_screen =
        located_event->target()->GetScreenLocation(*located_event);
    if (bounds_in_screen.Contains(event_location_in_screen))
      return true;
  }

  // Otherwise, cancel the event and close the container.
  return false;
}

void WebUITabStripContainerView::CloseForEventOutsideTabStrip() {
  RecordTabStripUICloseHistogram(TabStripUICloseAction::kTapOutsideTabStrip);
  iph_tracker_->NotifyEvent(feature_engagement::events::kWebUITabStripClosed);
  SetContainerTargetVisibility(false);
}

void WebUITabStripContainerView::AnimationEnded(
    const gfx::Animation* animation) {
  DCHECK_EQ(&animation_, animation);
  PreferredSizeChanged();
  if (animation_.GetCurrentValue() == 0.0)
    SetVisible(false);
}

void WebUITabStripContainerView::AnimationProgressed(
    const gfx::Animation* animation) {
  PreferredSizeChanged();
}

void WebUITabStripContainerView::ShowContextMenuAtPoint(
    gfx::Point point,
    std::unique_ptr<ui::MenuModel> menu_model) {
  ConvertPointToScreen(this, &point);
  context_menu_model_ = std::move(menu_model);
  context_menu_runner_ = std::make_unique<views::MenuRunner>(
      context_menu_model_.get(),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);
  context_menu_runner_->RunMenuAt(
      GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
      views::MenuAnchorPosition::kTopLeft, ui::MENU_SOURCE_MOUSE);
}

TabStripUILayout WebUITabStripContainerView::GetLayout() {
  DCHECK(tab_contents_container_);
  return TabStripUILayout::CalculateForWebViewportSize(
      tab_contents_container_->size());
}

const ui::ThemeProvider* WebUITabStripContainerView::GetThemeProvider() {
  return View::GetThemeProvider();
}

void WebUITabStripContainerView::AddedToWidget() {
  GetWidget()->GetNativeView()->AddPreTargetHandler(auto_closer_.get());
}

void WebUITabStripContainerView::RemovedFromWidget() {
  aura::Window* const native_view = GetWidget()->GetNativeView();
  if (native_view)
    native_view->RemovePreTargetHandler(auto_closer_.get());
}

int WebUITabStripContainerView::GetHeightForWidth(int w) const {
  if (!GetVisible())
    return 0;
  if (!animation_.is_animating())
    return desired_height_;

  return gfx::Tween::LinearIntValueBetween(animation_.GetCurrentValue(), 0,
                                           desired_height_);
}

void WebUITabStripContainerView::ButtonPressed(views::Button* sender,
                                               const ui::Event& event) {
  if (sender->GetID() == VIEW_ID_WEBUI_TAB_STRIP_TAB_COUNTER) {
    const bool new_visibility = !GetVisible();
    if (new_visibility) {
      RecordTabStripUIOpenHistogram(TabStripUIOpenAction::kTapOnTabCounter);
      iph_tracker_->NotifyEvent(
          feature_engagement::events::kWebUITabStripOpened);
    } else {
      RecordTabStripUICloseHistogram(TabStripUICloseAction::kTapOnTabCounter);
      iph_tracker_->NotifyEvent(
          feature_engagement::events::kWebUITabStripClosed);
    }

    SetContainerTargetVisibility(new_visibility);

    if (GetVisible() && sender->HasFocus()) {
      // Automatically move focus to the tab strip WebUI if the focus is
      // currently on the toggle button.
      SetPaneFocusAndFocusDefault();
    }
  } else if (sender->GetID() == VIEW_ID_WEBUI_TAB_STRIP_NEW_TAB_BUTTON) {
    chrome::ExecuteCommand(browser_, IDC_NEW_TAB);
    UMA_HISTOGRAM_ENUMERATION(
        "Tab.NewTab", TabStripModel::NEW_TAB_BUTTON_IN_TOOLBAR_FOR_TOUCH,
        TabStripModel::NEW_TAB_ENUM_COUNT);

    if (iph_tracker_->ShouldTriggerHelpUI(
            feature_engagement::kIPHWebUITabStripFeature)) {
      DCHECK(tab_counter_);
      tab_counter_->SetProperty(kHasInProductHelpPromoKey, true);
      tab_counter_promo_ = FeaturePromoBubbleView::CreateOwned(
          tab_counter_, views::BubbleBorder::TOP_RIGHT,
          FeaturePromoBubbleView::ActivationAction::DO_NOT_ACTIVATE,
          IDS_WEBUI_TAB_STRIP_PROMO);
      tab_counter_promo_->set_close_on_deactivate(false);
      widget_observer_.Add(tab_counter_promo_->GetWidget());
    }
  } else {
    NOTREACHED();
  }
}

void WebUITabStripContainerView::OnViewBoundsChanged(View* observed_view) {
  if (observed_view != tab_contents_container_)
    return;

  desired_height_ =
      TabStripUILayout::CalculateForWebViewportSize(observed_view->size())
          .CalculateContainerHeight();
  // TODO(pbos): PreferredSizeChanged seems to cause infinite recursion with
  // BrowserView::ChildPreferredSizeChanged. InvalidateLayout here should be
  // replaceable with PreferredSizeChanged.
  InvalidateLayout();

  TabStripUI* const tab_strip_ui = static_cast<TabStripUI*>(
      web_view_->GetWebContents()->GetWebUI()->GetController());
  tab_strip_ui->LayoutChanged();
}

void WebUITabStripContainerView::OnViewIsDeleting(View* observed_view) {
  view_observer_.Remove(observed_view);

  if (observed_view == new_tab_button_)
    new_tab_button_ = nullptr;
  else if (observed_view == tab_counter_)
    tab_counter_ = nullptr;
  else if (observed_view == tab_contents_container_)
    tab_contents_container_ = nullptr;
  else
    NOTREACHED();
}

void WebUITabStripContainerView::OnWidgetDestroying(views::Widget* widget) {
  // This call should only happen at the end of IPH.
  DCHECK_EQ(widget, tab_counter_promo_->GetWidget());
  tab_counter_promo_ = nullptr;
  widget_observer_.Remove(widget);

  tab_counter_->SetProperty(kHasInProductHelpPromoKey, false);
  iph_tracker_->Dismissed(feature_engagement::kIPHWebUITabStripFeature);
}

bool WebUITabStripContainerView::SetPaneFocusAndFocusDefault() {
  // Make sure the pane first receives focus, then send a WebUI event to the
  // front-end so the correct HTML element receives focus.
  bool received_focus = AccessiblePaneView::SetPaneFocusAndFocusDefault();
  if (received_focus) {
    TabStripUI* const tab_strip_ui = static_cast<TabStripUI*>(
        web_view_->GetWebContents()->GetWebUI()->GetController());
    tab_strip_ui->ReceivedKeyboardFocus();
  }
  return received_focus;
}
