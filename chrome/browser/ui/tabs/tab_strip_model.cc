// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/numerics/ranges.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_desktop_util.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/tabs/tab_group_data.h"
#include "chrome/browser/ui/tabs/tab_group_id.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_order_controller.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/web_contents_sizer.h"
#include "chrome/common/url_constants.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/feature_engagement/buildflags.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

#if BUILDFLAG(ENABLE_LEGACY_DESKTOP_IN_PRODUCT_HELP)
#include "chrome/browser/feature_engagement/new_tab/new_tab_tracker.h"
#include "chrome/browser/feature_engagement/new_tab/new_tab_tracker_factory.h"
#endif

using base::UserMetricsAction;
using content::WebContents;

namespace {

class RenderWidgetHostVisibilityTracker;

// Returns true if the specified transition is one of the types that cause the
// opener relationships for the tab in which the transition occurred to be
// forgotten. This is generally any navigation that isn't a link click (i.e.
// any navigation that can be considered to be the start of a new task distinct
// from what had previously occurred in that tab).
bool ShouldForgetOpenersForTransition(ui::PageTransition transition) {
  return ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_AUTO_BOOKMARK) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_GENERATED) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_KEYWORD) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
}

// Intalls RenderWidgetVisibilityTracker when the active tab has changed.
std::unique_ptr<RenderWidgetHostVisibilityTracker>
InstallRenderWigetVisibilityTracker(const TabStripSelectionChange& selection) {
  if (!selection.active_tab_changed())
    return nullptr;

  content::RenderWidgetHost* track_host = nullptr;
  if (selection.new_contents &&
      selection.new_contents->GetRenderWidgetHostView()) {
    track_host = selection.new_contents->GetRenderWidgetHostView()
                     ->GetRenderWidgetHost();
  }
  return std::make_unique<RenderWidgetHostVisibilityTracker>(track_host);
}

// This tracks (and reports via UMA and tracing) how long it takes before a
// RenderWidgetHost is requested to become visible.
class RenderWidgetHostVisibilityTracker
    : public content::RenderWidgetHostObserver {
 public:
  explicit RenderWidgetHostVisibilityTracker(content::RenderWidgetHost* host)
      : host_(host) {
    if (!host_ || host_->GetView()->IsShowing())
      return;
    host_->AddObserver(this);
    TRACE_EVENT_ASYNC_BEGIN0("ui,latency", "TabSwitchVisibilityRequest", this);
  }

  ~RenderWidgetHostVisibilityTracker() override {
    if (host_)
      host_->RemoveObserver(this);
  }

 private:
  // content::RenderWidgetHostObserver:
  void RenderWidgetHostVisibilityChanged(content::RenderWidgetHost* host,
                                         bool became_visible) override {
    DCHECK_EQ(host_, host);
    DCHECK(became_visible);
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Browser.Tabs.SelectionToVisibilityRequestTime", timer_.Elapsed(),
        base::TimeDelta::FromMicroseconds(1), base::TimeDelta::FromSeconds(3),
        50);
    TRACE_EVENT_ASYNC_END0("ui,latency", "TabSwitchVisibilityRequest", this);
  }

  void RenderWidgetHostDestroyed(content::RenderWidgetHost* host) override {
    DCHECK_EQ(host_, host);
    host_->RemoveObserver(this);
    host_ = nullptr;
  }

  content::RenderWidgetHost* host_ = nullptr;
  base::ElapsedTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostVisibilityTracker);
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// WebContentsData

// An object to own a WebContents that is in a tabstrip, as well as other
// various properties it has.
class TabStripModel::WebContentsData : public content::WebContentsObserver {
 public:
  explicit WebContentsData(std::unique_ptr<WebContents> a_contents);

  // Changes the WebContents that this WebContentsData tracks.
  std::unique_ptr<WebContents> ReplaceWebContents(
      std::unique_ptr<WebContents> contents);
  WebContents* web_contents() { return contents_.get(); }

  // See comments on fields.
  WebContents* opener() const { return opener_; }
  void set_opener(WebContents* value) { opener_ = value; }
  void set_reset_opener_on_active_tab_change(bool value) {
    reset_opener_on_active_tab_change_ = value;
  }
  bool reset_opener_on_active_tab_change() const {
    return reset_opener_on_active_tab_change_;
  }
  bool pinned() const { return pinned_; }
  void set_pinned(bool value) { pinned_ = value; }
  bool blocked() const { return blocked_; }
  void set_blocked(bool value) { blocked_ = value; }
  base::Optional<TabGroupId> group() const { return group_; }
  void set_group(base::Optional<TabGroupId> value) { group_ = value; }

 private:
  // Make sure that if someone deletes this WebContents out from under us, it
  // is properly removed from the tab strip.
  void WebContentsDestroyed() override;

  // The WebContents owned by this WebContentsData.
  std::unique_ptr<WebContents> contents_;

  // The opener is used to model a set of tabs spawned from a single parent tab.
  // The relationship is discarded easily, e.g. when the user switches to a tab
  // not part of the set. This property is used to determine what tab to
  // activate next when one is closed.
  WebContents* opener_ = nullptr;

  // True if |opener_| should be reset when any active tab change occurs (rather
  // than just one outside the current tree of openers).
  bool reset_opener_on_active_tab_change_ = false;

  // Whether the tab is pinned.
  bool pinned_ = false;

  // Whether the tab interaction is blocked by a modal dialog.
  bool blocked_ = false;

  // The group that contains this tab, if any.
  // TODO(https://crbug.com/915956): While tab groups are being prototyped
  // (behind a feature flag), we are tracking group membership in the simplest
  // possible way. There are some known issues intentionally punted here:
  //   - Groups are meant to be contiguous, but this data organization doesn't
  //     help to ensure that they stay contiguous. Any kind of tab movement may
  //     break that guarantee, with undefined results.
  //   - The exact shape of the group-related changes to the TabStripModel API
  //     (and the relevant bits of the extension API) are TBD.
  base::Optional<TabGroupId> group_ = base::nullopt;

  DISALLOW_COPY_AND_ASSIGN(WebContentsData);
};

TabStripModel::WebContentsData::WebContentsData(
    std::unique_ptr<WebContents> contents)
    : content::WebContentsObserver(contents.get()),
      contents_(std::move(contents)) {}

std::unique_ptr<WebContents> TabStripModel::WebContentsData::ReplaceWebContents(
    std::unique_ptr<WebContents> contents) {
  contents_.swap(contents);
  Observe(contents_.get());
  return contents;
}

void TabStripModel::WebContentsData::WebContentsDestroyed() {
  // TODO(erikchen): Remove this NOTREACHED statement as well as the
  // WebContents observer - this is just a temporary sanity check to make sure
  // that unit tests are not destroyed a WebContents out from under a
  // TabStripModel.
  NOTREACHED();
}

// Holds state for a WebContents that has been detached from the tab strip.
struct TabStripModel::DetachedWebContents {
  DetachedWebContents(int index_before_any_removals,
                      int index_at_time_of_removal,
                      std::unique_ptr<WebContents> contents)
      : contents(std::move(contents)),
        index_before_any_removals(index_before_any_removals),
        index_at_time_of_removal(index_at_time_of_removal) {}
  ~DetachedWebContents() = default;
  DetachedWebContents(DetachedWebContents&&) = default;

  std::unique_ptr<WebContents> contents;

  // The index of the WebContents in the original selection model of the tab
  // strip [prior to any tabs being removed, if multiple tabs are being
  // simultaneously removed].
  const int index_before_any_removals;

  // The index of the WebContents at the time it is being removed. If multiple
  // tabs are being simultaneously removed, the index reflects previously
  // removed tabs in this batch.
  const int index_at_time_of_removal;

 private:
  DISALLOW_COPY_AND_ASSIGN(DetachedWebContents);
};

// Holds all state necessary to send notifications for detached tabs. Will
// also handle WebContents deletion if |will_delete| is true.
struct TabStripModel::DetachNotifications {
  DetachNotifications(WebContents* initially_active_web_contents,
                      const ui::ListSelectionModel& selection_model,
                      bool will_delete)
      : initially_active_web_contents(initially_active_web_contents),
        selection_model(selection_model),
        will_delete(will_delete) {}
  ~DetachNotifications() = default;

  // The WebContents that was active prior to any detaches happening.
  //
  // It's safe to use a raw pointer here because the active web contents, if
  // detached, is owned by |detached_web_contents|.
  //
  // Once the notification for change of active web contents has been sent,
  // this field is set to nullptr.
  WebContents* initially_active_web_contents = nullptr;

  // The WebContents that were recently detached. Observers need to be notified
  // about these. These must be updated after construction.
  std::vector<std::unique_ptr<DetachedWebContents>> detached_web_contents;

  // The selection model prior to any tabs being detached.
  const ui::ListSelectionModel selection_model;

  // Whether to delete the WebContents after sending notifications.
  const bool will_delete;

 private:
  DISALLOW_COPY_AND_ASSIGN(DetachNotifications);
};

///////////////////////////////////////////////////////////////////////////////
// TabStripModel, public:

TabStripModel::TabStripModel(TabStripModelDelegate* delegate, Profile* profile)
    : delegate_(delegate), profile_(profile) {
  DCHECK(delegate_);
  order_controller_.reset(new TabStripModelOrderController(this));

  constexpr base::TimeDelta kTabScrubbingHistogramIntervalTime =
      base::TimeDelta::FromSeconds(30);

  last_tab_switch_timestamp_ = base::TimeTicks::Now();
  tab_scrubbing_interval_timer_.Start(
      FROM_HERE, kTabScrubbingHistogramIntervalTime,
      base::BindRepeating(&TabStripModel::RecordTabScrubbingMetrics,
                          base::Unretained(this)));
}

TabStripModel::~TabStripModel() {
  contents_data_.clear();
  order_controller_.reset();
}

void TabStripModel::SetTabStripUI(TabStripModelObserver* observer) {
  DCHECK(!tab_strip_ui_was_set_);

  std::vector<TabStripModelObserver*> new_observers{observer};
  for (auto& old_observer : observers_)
    new_observers.push_back(&old_observer);

  observers_.Clear();

  for (auto* new_observer : new_observers)
    observers_.AddObserver(new_observer);

  tab_strip_ui_was_set_ = true;
}

void TabStripModel::AddObserver(TabStripModelObserver* observer) {
  observers_.AddObserver(observer);
}

void TabStripModel::RemoveObserver(TabStripModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool TabStripModel::ContainsIndex(int index) const {
  return index >= 0 && index < count();
}

void TabStripModel::AppendWebContents(std::unique_ptr<WebContents> contents,
                                      bool foreground) {
  InsertWebContentsAt(
      count(), std::move(contents),
      foreground ? (ADD_INHERIT_OPENER | ADD_ACTIVE) : ADD_NONE);
}

int TabStripModel::InsertWebContentsAt(int index,
                                       std::unique_ptr<WebContents> contents,
                                       int add_types,
                                       base::Optional<TabGroupId> group) {
  DCHECK(!reentrancy_guard_);
  base::AutoReset<bool> resetter(&reentrancy_guard_, true);

  return InsertWebContentsAtImpl(index, std::move(contents), add_types, group);
}

std::unique_ptr<content::WebContents> TabStripModel::ReplaceWebContentsAt(
    int index,
    std::unique_ptr<WebContents> new_contents) {
  DCHECK(!reentrancy_guard_);
  base::AutoReset<bool> resetter(&reentrancy_guard_, true);

  delegate()->WillAddWebContents(new_contents.get());

  DCHECK(ContainsIndex(index));

  FixOpeners(index);

  TabStripSelectionChange selection(GetActiveWebContents(), selection_model_);
  WebContents* raw_new_contents = new_contents.get();
  std::unique_ptr<WebContents> old_contents =
      contents_data_[index]->ReplaceWebContents(std::move(new_contents));

  // When the active WebContents is replaced send out a selection notification
  // too. We do this as nearly all observers need to treat a replacement of the
  // selected contents as the selection changing.
  if (active_index() == index) {
    selection.new_contents = raw_new_contents;
    selection.reason = TabStripModelObserver::CHANGE_REASON_REPLACED;
  }

  TabStripModelChange::Replace replace;
  replace.old_contents = old_contents.get();
  replace.new_contents = raw_new_contents;
  replace.index = index;
  TabStripModelChange change(replace);
  for (auto& observer : observers_)
    observer.OnTabStripModelChanged(this, change, selection);

  return old_contents;
}

std::unique_ptr<content::WebContents> TabStripModel::DetachWebContentsAt(
    int index) {
  DCHECK(!reentrancy_guard_);
  base::AutoReset<bool> resetter(&reentrancy_guard_, true);

  DCHECK_NE(active_index(), kNoTab) << "Activate the TabStripModel by "
                                       "selecting at least one tab before "
                                       "trying to detach web contents.";
  WebContents* initially_active_web_contents =
      GetWebContentsAtImpl(active_index());

  DetachNotifications notifications(initially_active_web_contents,
                                    selection_model_, /*will_delete=*/false);
  std::unique_ptr<DetachedWebContents> dwc =
      std::make_unique<DetachedWebContents>(
          index, index,
          DetachWebContentsImpl(index, /*create_historical_tab=*/false,
                                /*will_delete=*/false));
  notifications.detached_web_contents.push_back(std::move(dwc));
  SendDetachWebContentsNotifications(&notifications);
  return std::move(notifications.detached_web_contents[0]->contents);
}

std::unique_ptr<content::WebContents> TabStripModel::DetachWebContentsImpl(
    int index,
    bool create_historical_tab,
    bool will_delete) {
  if (contents_data_.empty())
    return nullptr;
  DCHECK(ContainsIndex(index));

  FixOpeners(index);

  // Ask the delegate to save an entry for this tab in the historical tab
  // database.
  WebContents* raw_web_contents = GetWebContentsAtImpl(index);
  if (create_historical_tab)
    delegate_->CreateHistoricalTab(raw_web_contents);

  NotifyGroupChange(index, UngroupTab(index), base::nullopt);

  int next_selected_index = order_controller_->DetermineNewSelectedIndex(index);
  std::unique_ptr<WebContentsData> old_data = std::move(contents_data_[index]);
  contents_data_.erase(contents_data_.begin() + index);

  if (empty()) {
    selection_model_.Clear();
  } else {
    int old_active = active_index();
    selection_model_.DecrementFrom(index);
    ui::ListSelectionModel old_model;
    old_model = selection_model_;
    if (index == old_active) {
      if (!selection_model_.empty()) {
        // The active tab was removed, but there is still something selected.
        // Move the active and anchor to the first selected index.
        selection_model_.set_active(selection_model_.selected_indices()[0]);
        selection_model_.set_anchor(selection_model_.active());
      } else {
        // The active tab was removed and nothing is selected. Reset the
        // selection and send out notification.
        selection_model_.SetSelectedIndex(next_selected_index);
      }
    }
  }
  return old_data->ReplaceWebContents(nullptr);
}

void TabStripModel::SendDetachWebContentsNotifications(
    DetachNotifications* notifications) {
  // Sort the DetachedWebContents in decreasing order of
  // |index_before_any_removals|. This is because |index_before_any_removals| is
  // used by observers to update their own copy of TabStripModel state, and each
  // removal affects subsequent removals of higher index.
  std::sort(notifications->detached_web_contents.begin(),
            notifications->detached_web_contents.end(),
            [](const std::unique_ptr<DetachedWebContents>& dwc1,
               const std::unique_ptr<DetachedWebContents>& dwc2) {
              return dwc1->index_before_any_removals >
                     dwc2->index_before_any_removals;
            });

  TabStripModelChange::Remove remove;
  remove.will_be_deleted = notifications->will_delete;
  for (auto& dwc : notifications->detached_web_contents) {
    remove.contents.push_back(
        {dwc->contents.get(), dwc->index_before_any_removals});
  }
  TabStripModelChange change(std::move(remove));

  TabStripSelectionChange selection;
  selection.old_contents = notifications->initially_active_web_contents;
  selection.new_contents = GetActiveWebContents();
  selection.old_model = notifications->selection_model;
  selection.new_model = selection_model_;
  selection.reason = TabStripModelObserver::CHANGE_REASON_NONE;
  selection.selected_tabs_were_removed = std::any_of(
      notifications->detached_web_contents.begin(),
      notifications->detached_web_contents.end(), [&notifications](auto& dwc) {
        return notifications->selection_model.IsSelected(
            dwc->index_before_any_removals);
      });

  {
    auto visibility_tracker =
        empty() ? nullptr : InstallRenderWigetVisibilityTracker(selection);
    for (auto& observer : observers_)
      observer.OnTabStripModelChanged(this, change, selection);
  }

  for (auto& dwc : notifications->detached_web_contents) {
    if (notifications->will_delete) {
      // This destroys the WebContents, which will also send
      // WebContentsDestroyed notifications.
      dwc->contents.reset();
    }
  }

  if (empty()) {
    for (auto& observer : observers_)
      observer.TabStripEmpty();
  }
}

void TabStripModel::ActivateTabAt(int index, UserGestureDetails user_gesture) {
  DCHECK(!reentrancy_guard_);
  base::AutoReset<bool> resetter(&reentrancy_guard_, true);

  DCHECK(ContainsIndex(index));
  TRACE_EVENT0("ui", "TabStripModel::ActivateTabAt");

  // Maybe increment count of tabs 'scrubbed' by mouse or key press for
  // histogram data.
  if (user_gesture.type == GestureType::kMouse ||
      user_gesture.type == GestureType::kKeyboard) {
    constexpr base::TimeDelta kMaxTimeConsideredScrubbing =
        base::TimeDelta::FromMilliseconds(1500);
    base::TimeDelta elapsed_time_since_tab_switch =
        base::TimeTicks::Now() - last_tab_switch_timestamp_;
    if (elapsed_time_since_tab_switch <= kMaxTimeConsideredScrubbing) {
      if (user_gesture.type == GestureType::kMouse)
        ++tabs_scrubbed_by_mouse_press_count_;
      else if (user_gesture.type == GestureType::kKeyboard)
        ++tabs_scrubbed_by_key_press_count_;
    }
  }
  last_tab_switch_timestamp_ = base::TimeTicks::Now();

  TabSwitchEventLatencyRecorder::EventType event_type;
  switch (user_gesture.type) {
    case GestureType::kMouse:
      event_type = TabSwitchEventLatencyRecorder::EventType::kMouse;
      break;
    case GestureType::kKeyboard:
      event_type = TabSwitchEventLatencyRecorder::EventType::kKeyboard;
      break;
    case GestureType::kTouch:
      event_type = TabSwitchEventLatencyRecorder::EventType::kTouch;
      break;
    case GestureType::kWheel:
      event_type = TabSwitchEventLatencyRecorder::EventType::kWheel;
      break;
    default:
      event_type = TabSwitchEventLatencyRecorder::EventType::kOther;
      break;
  }
  tab_switch_event_latency_recorder_.BeginLatencyTiming(user_gesture.time_stamp,
                                                        event_type);
  ui::ListSelectionModel new_model = selection_model_;
  new_model.SetSelectedIndex(index);
  SetSelection(std::move(new_model),
               user_gesture.type != GestureType::kNone
                   ? TabStripModelObserver::CHANGE_REASON_USER_GESTURE
                   : TabStripModelObserver::CHANGE_REASON_NONE,
               /*triggered_by_other_operation=*/false);
}

void TabStripModel::RecordTabScrubbingMetrics() {
  UMA_HISTOGRAM_COUNTS_10000("Tabs.ScrubbedInInterval.MousePress",
                             tabs_scrubbed_by_mouse_press_count_);
  UMA_HISTOGRAM_COUNTS_10000("Tabs.ScrubbedInInterval.KeyPress",
                             tabs_scrubbed_by_key_press_count_);
  tabs_scrubbed_by_mouse_press_count_ = 0;
  tabs_scrubbed_by_key_press_count_ = 0;
}

int TabStripModel::MoveWebContentsAt(int index,
                                     int to_position,
                                     bool select_after_move) {
  DCHECK(!reentrancy_guard_);
  base::AutoReset<bool> resetter(&reentrancy_guard_, true);

  DCHECK(ContainsIndex(index));

  // Ensure pinned and non-pinned tabs do not mix.
  const int first_non_pinned_tab = IndexOfFirstNonPinnedTab();
  to_position = IsTabPinned(index)
                    ? std::min(first_non_pinned_tab - 1, to_position)
                    : std::max(first_non_pinned_tab, to_position);

  if (index != to_position)
    MoveWebContentsAtImpl(index, to_position, select_after_move);
  return to_position;
}

void TabStripModel::MoveSelectedTabsTo(int index) {
  DCHECK(!reentrancy_guard_);
  base::AutoReset<bool> resetter(&reentrancy_guard_, true);

  int total_pinned_count = IndexOfFirstNonPinnedTab();
  int selected_pinned_count = 0;
  int selected_count =
      static_cast<int>(selection_model_.selected_indices().size());
  for (int i = 0; i < selected_count &&
                  IsTabPinned(selection_model_.selected_indices()[i]);
       ++i) {
    selected_pinned_count++;
  }

  // To maintain that all pinned tabs occur before non-pinned tabs we move them
  // first.
  if (selected_pinned_count > 0) {
    MoveSelectedTabsToImpl(
        std::min(total_pinned_count - selected_pinned_count, index), 0u,
        selected_pinned_count);
    if (index > total_pinned_count - selected_pinned_count) {
      // We're being told to drag pinned tabs to an invalid location. Adjust the
      // index such that non-pinned tabs end up at a location as though we could
      // move the pinned tabs to index. See description in header for more
      // details.
      index += selected_pinned_count;
    }
  }
  if (selected_pinned_count == selected_count)
    return;

  // Then move the non-pinned tabs.
  MoveSelectedTabsToImpl(std::max(index, total_pinned_count),
                         selected_pinned_count,
                         selected_count - selected_pinned_count);
}

WebContents* TabStripModel::GetActiveWebContents() const {
  return GetWebContentsAt(active_index());
}

WebContents* TabStripModel::GetWebContentsAt(int index) const {
  if (ContainsIndex(index))
    return GetWebContentsAtImpl(index);
  return nullptr;
}

int TabStripModel::GetIndexOfWebContents(const WebContents* contents) const {
  for (size_t i = 0; i < contents_data_.size(); ++i) {
    if (contents_data_[i]->web_contents() == contents)
      return i;
  }
  return kNoTab;
}

void TabStripModel::UpdateWebContentsStateAt(int index,
                                             TabChangeType change_type) {
  DCHECK(ContainsIndex(index));

  for (auto& observer : observers_)
    observer.TabChangedAt(GetWebContentsAtImpl(index), index, change_type);
}

void TabStripModel::SetTabNeedsAttentionAt(int index, bool attention) {
  DCHECK(ContainsIndex(index));

  for (auto& observer : observers_)
    observer.SetTabNeedsAttentionAt(index, attention);
}

void TabStripModel::CloseAllTabs() {
  DCHECK(!reentrancy_guard_);
  base::AutoReset<bool> resetter(&reentrancy_guard_, true);

  // Set state so that observers can adjust their behavior to suit this
  // specific condition when CloseWebContentsAt causes a flurry of
  // Close/Detach/Select notifications to be sent.
  closing_all_ = true;
  std::vector<content::WebContents*> closing_tabs;
  closing_tabs.reserve(count());
  for (int i = count() - 1; i >= 0; --i)
    closing_tabs.push_back(GetWebContentsAt(i));
  InternalCloseTabs(closing_tabs, CLOSE_CREATE_HISTORICAL_TAB);
}

bool TabStripModel::CloseWebContentsAt(int index, uint32_t close_types) {
  DCHECK(ContainsIndex(index));
  WebContents* contents = GetWebContentsAt(index);
  return InternalCloseTabs(base::span<WebContents* const>(&contents, 1),
                           close_types);
}

bool TabStripModel::TabsAreLoading() const {
  for (const auto& data : contents_data_) {
    if (data->web_contents()->IsLoading())
      return true;
  }

  return false;
}

WebContents* TabStripModel::GetOpenerOfWebContentsAt(int index) {
  DCHECK(ContainsIndex(index));
  return contents_data_[index]->opener();
}

void TabStripModel::SetOpenerOfWebContentsAt(int index, WebContents* opener) {
  DCHECK(ContainsIndex(index));
  // The TabStripModel only maintains the references to openers that it itself
  // owns; trying to set an opener to an external WebContents can result in
  // the opener being used after its freed. See crbug.com/698681.
  DCHECK(!opener || GetIndexOfWebContents(opener) != kNoTab)
      << "Cannot set opener to a web contents not owned by this tab strip.";
  contents_data_[index]->set_opener(opener);
}

int TabStripModel::GetIndexOfLastWebContentsOpenedBy(const WebContents* opener,
                                                     int start_index) const {
  DCHECK(opener);
  DCHECK(ContainsIndex(start_index));

  std::set<const WebContents*> opener_and_descendants;
  opener_and_descendants.insert(opener);
  int last_index = kNoTab;

  for (int i = start_index + 1; i < count(); ++i) {
    // Test opened by transitively, i.e. include tabs opened by tabs opened by
    // opener, etc. Stop when we find the first non-descendant.
    if (!opener_and_descendants.count(contents_data_[i]->opener())) {
      // Skip over pinned tabs as new tabs are added after pinned tabs.
      if (contents_data_[i]->pinned())
        continue;
      break;
    }
    opener_and_descendants.insert(contents_data_[i]->web_contents());
    last_index = i;
  }
  return last_index;
}

void TabStripModel::TabNavigating(WebContents* contents,
                                  ui::PageTransition transition) {
  if (ShouldForgetOpenersForTransition(transition)) {
    // Don't forget the openers if this tab is a New Tab page opened at the
    // end of the TabStrip (e.g. by pressing Ctrl+T). Give the user one
    // navigation of one of these transition types before resetting the
    // opener relationships (this allows for the use case of opening a new
    // tab to do a quick look-up of something while viewing a tab earlier in
    // the strip). We can make this heuristic more permissive if need be.
    if (!IsNewTabAtEndOfTabStrip(contents)) {
      // If the user navigates the current tab to another page in any way
      // other than by clicking a link, we want to pro-actively forget all
      // TabStrip opener relationships since we assume they're beginning a
      // different task by reusing the current tab.
      ForgetAllOpeners();
    }
  }
}

void TabStripModel::SetTabBlocked(int index, bool blocked) {
  DCHECK(ContainsIndex(index));
  if (contents_data_[index]->blocked() == blocked)
    return;
  contents_data_[index]->set_blocked(blocked);
  for (auto& observer : observers_)
    observer.TabBlockedStateChanged(contents_data_[index]->web_contents(),
                                    index);
}

void TabStripModel::SetTabPinned(int index, bool pinned) {
  DCHECK(!reentrancy_guard_);
  base::AutoReset<bool> resetter(&reentrancy_guard_, true);

  SetTabPinnedImpl(index, pinned);
}

bool TabStripModel::IsTabPinned(int index) const {
  DCHECK(ContainsIndex(index)) << index;
  return contents_data_[index]->pinned();
}

bool TabStripModel::IsTabBlocked(int index) const {
  return contents_data_[index]->blocked();
}

const TabGroupData* TabStripModel::GetDataForGroup(TabGroupId group) const {
  DCHECK(base::Contains(group_data_, group));
  return &group_data_.at(group);
}

base::Optional<TabGroupId> TabStripModel::GetTabGroupForTab(int index) const {
  return ContainsIndex(index) ? contents_data_[index]->group() : base::nullopt;
}

std::vector<TabGroupId> TabStripModel::ListTabGroups() const {
  std::vector<TabGroupId> groups;
  groups.reserve(group_data_.size());
  for (const auto& id_group_pair : group_data_)
    groups.push_back(id_group_pair.first);
  return groups;
}

std::vector<int> TabStripModel::ListTabsInGroup(TabGroupId group) const {
  std::vector<int> result;
  for (size_t i = 0; i < contents_data_.size(); ++i) {
    if (contents_data_[i]->group() == group)
      result.push_back(i);
  }
  return result;
}

bool TabStripModel::IsGroupPinned(TabGroupId group) const {
  for (const auto& contents_datum : contents_data_) {
    if (contents_datum->group() == group)
      return contents_datum->pinned();
  }
  NOTREACHED();
  return false;
}

int TabStripModel::IndexOfFirstNonPinnedTab() const {
  for (size_t i = 0; i < contents_data_.size(); ++i) {
    if (!IsTabPinned(static_cast<int>(i)))
      return static_cast<int>(i);
  }
  // No pinned tabs.
  return count();
}

void TabStripModel::ExtendSelectionTo(int index) {
  DCHECK(ContainsIndex(index));
  ui::ListSelectionModel new_model = selection_model_;
  new_model.SetSelectionFromAnchorTo(index);
  SetSelection(std::move(new_model), TabStripModelObserver::CHANGE_REASON_NONE,
               /*triggered_by_other_operation=*/false);
}

void TabStripModel::ToggleSelectionAt(int index) {
  DCHECK(ContainsIndex(index));
  ui::ListSelectionModel new_model = selection_model();
  if (selection_model_.IsSelected(index)) {
    if (selection_model_.size() == 1) {
      // One tab must be selected and this tab is currently selected so we can't
      // unselect it.
      return;
    }
    new_model.RemoveIndexFromSelection(index);
    new_model.set_anchor(index);
    if (new_model.active() == index ||
        new_model.active() == ui::ListSelectionModel::kUnselectedIndex)
      new_model.set_active(new_model.selected_indices()[0]);
  } else {
    new_model.AddIndexToSelection(index);
    new_model.set_anchor(index);
    new_model.set_active(index);
  }
  SetSelection(std::move(new_model), TabStripModelObserver::CHANGE_REASON_NONE,
               /*triggered_by_other_operation=*/false);
}

void TabStripModel::AddSelectionFromAnchorTo(int index) {
  ui::ListSelectionModel new_model = selection_model_;
  new_model.AddSelectionFromAnchorTo(index);
  SetSelection(std::move(new_model), TabStripModelObserver::CHANGE_REASON_NONE,
               /*triggered_by_other_operation=*/false);
}

bool TabStripModel::IsTabSelected(int index) const {
  DCHECK(ContainsIndex(index));
  return selection_model_.IsSelected(index);
}

void TabStripModel::SetSelectionFromModel(ui::ListSelectionModel source) {
  DCHECK_NE(ui::ListSelectionModel::kUnselectedIndex, source.active());
  SetSelection(std::move(source), TabStripModelObserver::CHANGE_REASON_NONE,
               /*triggered_by_other_operation=*/false);
}

const ui::ListSelectionModel& TabStripModel::selection_model() const {
  return selection_model_;
}

void TabStripModel::AddWebContents(std::unique_ptr<WebContents> contents,
                                   int index,
                                   ui::PageTransition transition,
                                   int add_types,
                                   base::Optional<TabGroupId> group) {
  DCHECK(!reentrancy_guard_);
  base::AutoReset<bool> resetter(&reentrancy_guard_, true);

  // If the newly-opened tab is part of the same task as the parent tab, we want
  // to inherit the parent's opener attribute, so that if this tab is then
  // closed we'll jump back to the parent tab.
  bool inherit_opener = (add_types & ADD_INHERIT_OPENER) == ADD_INHERIT_OPENER;

  if (ui::PageTransitionTypeIncludingQualifiersIs(transition,
                                                  ui::PAGE_TRANSITION_LINK) &&
      (add_types & ADD_FORCE_INDEX) == 0) {
    // We assume tabs opened via link clicks are part of the same task as their
    // parent.  Note that when |force_index| is true (e.g. when the user
    // drag-and-drops a link to the tab strip), callers aren't really handling
    // link clicks, they just want to score the navigation like a link click in
    // the history backend, so we don't inherit the opener in this case.
    index = order_controller_->DetermineInsertionIndex(transition,
                                                       add_types & ADD_ACTIVE);
    inherit_opener = true;
  } else {
    // For all other types, respect what was passed to us, normalizing -1s and
    // values that are too large.
    if (index < 0 || index > count())
      index = count();
  }

  // Prevent the tab from being inserted at an index that would make the group
  // non-contiguous. Most commonly, the new-tab button always attempts to insert
  // at the end of the tab strip. Extensions can insert at an arbitrary index,
  // so we have to handle the general case.
  if (group.has_value()) {
    auto grouped_tabs = ListTabsInGroup(group.value());
    DCHECK(base::STLIsSorted(grouped_tabs));
    index = base::ClampToRange(index, grouped_tabs.front(),
                               grouped_tabs.back() + 1);
  }

  if (ui::PageTransitionTypeIncludingQualifiersIs(transition,
                                                  ui::PAGE_TRANSITION_TYPED) &&
      index == count()) {
    // Also, any tab opened at the end of the TabStrip with a "TYPED"
    // transition inherit opener as well. This covers the cases where the user
    // creates a New Tab (e.g. Ctrl+T, or clicks the New Tab button), or types
    // in the address bar and presses Alt+Enter. This allows for opening a new
    // Tab to quickly look up something. When this Tab is closed, the old one
    // is re-activated, not the next-adjacent.
    inherit_opener = true;
  }
  WebContents* raw_contents = contents.get();
  InsertWebContentsAtImpl(index, std::move(contents),
                          add_types | (inherit_opener ? ADD_INHERIT_OPENER : 0),
                          group);
  // Reset the index, just in case insert ended up moving it on us.
  index = GetIndexOfWebContents(raw_contents);

  // In the "quick look-up" case detailed above, we want to reset the opener
  // relationship on any active tab change, even to another tab in the same tree
  // of openers. A jump would be too confusing at that point.
  if (inherit_opener && ui::PageTransitionTypeIncludingQualifiersIs(
                            transition, ui::PAGE_TRANSITION_TYPED))
    contents_data_[index]->set_reset_opener_on_active_tab_change(true);

  // TODO(sky): figure out why this is here and not in InsertWebContentsAt. When
  // here we seem to get failures in startup perf tests.
  // Ensure that the new WebContentsView begins at the same size as the
  // previous WebContentsView if it existed.  Otherwise, the initial WebKit
  // layout will be performed based on a width of 0 pixels, causing a
  // very long, narrow, inaccurate layout.  Because some scripts on pages (as
  // well as WebKit's anchor link location calculation) are run on the
  // initial layout and not recalculated later, we need to ensure the first
  // layout is performed with sane view dimensions even when we're opening a
  // new background tab.
  if (WebContents* old_contents = GetActiveWebContents()) {
    if ((add_types & ADD_ACTIVE) == 0) {
      ResizeWebContents(raw_contents,
                        gfx::Rect(old_contents->GetContainerBounds().size()));
    }
  }
}

void TabStripModel::CloseSelectedTabs() {
  DCHECK(!reentrancy_guard_);
  base::AutoReset<bool> resetter(&reentrancy_guard_, true);

  InternalCloseTabs(
      GetWebContentsesByIndices(selection_model_.selected_indices()),
      CLOSE_CREATE_HISTORICAL_TAB | CLOSE_USER_GESTURE);
}

void TabStripModel::SelectNextTab(UserGestureDetails detail) {
  SelectRelativeTab(true, detail);
}

void TabStripModel::SelectPreviousTab(UserGestureDetails detail) {
  SelectRelativeTab(false, detail);
}

void TabStripModel::SelectLastTab(UserGestureDetails detail) {
  ActivateTabAt(count() - 1, detail);
}

void TabStripModel::MoveTabNext() {
  // TODO: this likely needs to be updated for multi-selection.
  int new_index = std::min(active_index() + 1, count() - 1);
  MoveWebContentsAt(active_index(), new_index, true);
}

void TabStripModel::MoveTabPrevious() {
  // TODO: this likely needs to be updated for multi-selection.
  int new_index = std::max(active_index() - 1, 0);
  MoveWebContentsAt(active_index(), new_index, true);
}

TabGroupId TabStripModel::AddToNewGroup(const std::vector<int>& indices) {
  DCHECK(!reentrancy_guard_);
  base::AutoReset<bool> resetter(&reentrancy_guard_, true);

  // The odds of |new_group| colliding with an existing group are astronomically
  // low. If there is a collision, a DCHECK will fail in |AddToNewGroupImpl()|,
  // in which case there is probably something wrong with
  // |TabGroupId::GenerateNew()|.
  const TabGroupId new_group = TabGroupId::GenerateNew();
  AddToNewGroupImpl(indices, new_group);
  return new_group;
}

void TabStripModel::AddToExistingGroup(const std::vector<int>& indices,
                                       TabGroupId group) {
  DCHECK(!reentrancy_guard_);
  base::AutoReset<bool> resetter(&reentrancy_guard_, true);

  AddToExistingGroupImpl(indices, group);
}

void TabStripModel::AddToGroupForRestore(const std::vector<int>& indices,
                                         TabGroupId group) {
  DCHECK(!reentrancy_guard_);
  base::AutoReset<bool> resetter(&reentrancy_guard_, true);

  const bool group_exists = base::Contains(group_data_, group);
  if (group_exists)
    AddToExistingGroupImpl(indices, group);
  else
    AddToNewGroupImpl(indices, group);
}

void TabStripModel::RemoveFromGroup(const std::vector<int>& indices) {
  DCHECK(!reentrancy_guard_);
  base::AutoReset<bool> resetter(&reentrancy_guard_, true);

  // Remove each tab from the group it's in, if any. Go from right to left
  // since tabs may move to the right.
  for (int i = indices.size() - 1; i >= 0; i--) {
    const int index = indices[i];
    base::Optional<TabGroupId> old_group = GetTabGroupForTab(index);
    if (!old_group.has_value())
      continue;

    // Move the tab until it's the rightmost tab in its group
    int new_index = index;
    while (ContainsIndex(new_index + 1) &&
           GetTabGroupForTab(new_index + 1) == old_group)
      new_index++;
    MoveAndSetGroup(index, new_index, base::nullopt);
  }
}

// Context menu functions.
bool TabStripModel::IsContextMenuCommandEnabled(
    int context_index,
    ContextMenuCommand command_id) const {
  DCHECK(command_id > CommandFirst && command_id < CommandLast);
  switch (command_id) {
    case CommandNewTab:
    case CommandCloseTab:
      return true;

    case CommandReload: {
      std::vector<int> indices = GetIndicesForCommand(context_index);
      for (size_t i = 0; i < indices.size(); ++i) {
        WebContents* tab = GetWebContentsAt(indices[i]);
        if (tab) {
          Browser* browser = chrome::FindBrowserWithWebContents(tab);
          if (!browser || browser->CanReloadContents(tab))
            return true;
        }
      }
      return false;
    }

    case CommandCloseOtherTabs:
    case CommandCloseTabsToRight:
      return !GetIndicesClosedByCommand(context_index, command_id).empty();

    case CommandDuplicate: {
      std::vector<int> indices = GetIndicesForCommand(context_index);
      for (size_t i = 0; i < indices.size(); ++i) {
        if (delegate()->CanDuplicateContentsAt(indices[i]))
          return true;
      }
      return false;
    }

    case CommandRestoreTab:
      return delegate()->GetRestoreTabType() !=
             TabStripModelDelegate::RESTORE_NONE;

    case CommandToggleSiteMuted: {
      std::vector<int> indices = GetIndicesForCommand(context_index);
      for (size_t i = 0; i < indices.size(); ++i) {
        if (!chrome::CanToggleAudioMute(GetWebContentsAt(indices[i])))
          return false;
      }
      return true;
    }

    case CommandBookmarkAllTabs:
      return browser_defaults::bookmarks_enabled &&
             delegate()->CanBookmarkAllTabs();

    case CommandTogglePinned:
      return true;

    case CommandFocusMode:
      return GetIndicesForCommand(context_index).size() == 1;

    case CommandSendTabToSelf:
      return true;

    case CommandSendTabToSelfSingleTarget:
      return true;

    case CommandAddToNewGroup:
      return true;

    case CommandAddToExistingGroup:
      return true;

    case CommandRemoveFromGroup:
      return true;

    default:
      NOTREACHED();
  }
  return false;
}

void TabStripModel::ExecuteContextMenuCommand(int context_index,
                                              ContextMenuCommand command_id) {
  DCHECK(command_id > CommandFirst && command_id < CommandLast);
  switch (command_id) {
    case CommandNewTab: {
      base::RecordAction(UserMetricsAction("TabContextMenu_NewTab"));
      UMA_HISTOGRAM_ENUMERATION("Tab.NewTab",
                                TabStripModel::NEW_TAB_CONTEXT_MENU,
                                TabStripModel::NEW_TAB_ENUM_COUNT);
      delegate()->AddTabAt(GURL(), context_index + 1, true,
                           GetTabGroupForTab(context_index));
#if BUILDFLAG(ENABLE_LEGACY_DESKTOP_IN_PRODUCT_HELP)
      auto* new_tab_tracker =
          feature_engagement::NewTabTrackerFactory::GetInstance()
              ->GetForProfile(profile_);
      new_tab_tracker->OnNewTabOpened();
      new_tab_tracker->CloseBubble();
#endif
      break;
    }

    case CommandReload: {
      base::RecordAction(UserMetricsAction("TabContextMenu_Reload"));
      std::vector<int> indices = GetIndicesForCommand(context_index);
      for (size_t i = 0; i < indices.size(); ++i) {
        WebContents* tab = GetWebContentsAt(indices[i]);
        if (tab) {
          Browser* browser = chrome::FindBrowserWithWebContents(tab);
          if (!browser || browser->CanReloadContents(tab))
            tab->GetController().Reload(content::ReloadType::NORMAL, true);
        }
      }
      break;
    }

    case CommandDuplicate: {
      base::RecordAction(UserMetricsAction("TabContextMenu_Duplicate"));
      std::vector<int> indices = GetIndicesForCommand(context_index);
      // Copy the WebContents off as the indices will change as tabs are
      // duplicated.
      std::vector<WebContents*> tabs;
      for (size_t i = 0; i < indices.size(); ++i)
        tabs.push_back(GetWebContentsAt(indices[i]));
      for (size_t i = 0; i < tabs.size(); ++i) {
        int index = GetIndexOfWebContents(tabs[i]);
        if (index != -1 && delegate()->CanDuplicateContentsAt(index))
          delegate()->DuplicateContentsAt(index);
      }
      break;
    }

    case CommandCloseTab: {
      DCHECK(!reentrancy_guard_);
      base::AutoReset<bool> resetter(&reentrancy_guard_, true);

      base::RecordAction(UserMetricsAction("TabContextMenu_CloseTab"));
      InternalCloseTabs(
          GetWebContentsesByIndices(GetIndicesForCommand(context_index)),
          CLOSE_CREATE_HISTORICAL_TAB | CLOSE_USER_GESTURE);
      break;
    }

    case CommandCloseOtherTabs: {
      DCHECK(!reentrancy_guard_);
      base::AutoReset<bool> resetter(&reentrancy_guard_, true);

      base::RecordAction(UserMetricsAction("TabContextMenu_CloseOtherTabs"));
      InternalCloseTabs(GetWebContentsesByIndices(GetIndicesClosedByCommand(
                            context_index, command_id)),
                        CLOSE_CREATE_HISTORICAL_TAB);
      break;
    }

    case CommandCloseTabsToRight: {
      DCHECK(!reentrancy_guard_);
      base::AutoReset<bool> resetter(&reentrancy_guard_, true);

      base::RecordAction(UserMetricsAction("TabContextMenu_CloseTabsToRight"));
      InternalCloseTabs(GetWebContentsesByIndices(GetIndicesClosedByCommand(
                            context_index, command_id)),
                        CLOSE_CREATE_HISTORICAL_TAB);
      break;
    }

    case CommandRestoreTab: {
      base::RecordAction(UserMetricsAction("TabContextMenu_RestoreTab"));
      delegate()->RestoreTab();
      break;
    }

    case CommandSendTabToSelfSingleTarget: {
      send_tab_to_self::ShareToSingleTarget(GetActiveWebContents());
      send_tab_to_self::RecordSendTabToSelfClickResult(
          send_tab_to_self::kTabMenu, SendTabToSelfClickResult::kClickItem);
      break;
    }

    case CommandTogglePinned: {
      DCHECK(!reentrancy_guard_);
      base::AutoReset<bool> resetter(&reentrancy_guard_, true);

      base::RecordAction(UserMetricsAction("TabContextMenu_TogglePinned"));
      std::vector<int> indices = GetIndicesForCommand(context_index);
      bool pin = WillContextMenuPin(context_index);
      if (pin) {
        for (size_t i = 0; i < indices.size(); ++i)
          SetTabPinnedImpl(indices[i], true);
      } else {
        // Unpin from the back so that the order is maintained (unpinning can
        // trigger moving a tab).
        for (size_t i = indices.size(); i > 0; --i)
          SetTabPinnedImpl(indices[i - 1], false);
      }
      break;
    }

    case CommandFocusMode: {
      base::RecordAction(UserMetricsAction("TabContextMenu_FocusMode"));
      std::vector<int> indices = GetIndicesForCommand(context_index);
      WebContents* contents = GetWebContentsAt(indices[0]);
      ReparentWebContentsForFocusMode(contents);
      break;
    }

    case CommandToggleSiteMuted: {
      const bool mute = WillContextMenuMuteSites(context_index);
      if (mute) {
        base::RecordAction(
            UserMetricsAction("SoundContentSetting.MuteBy.TabStrip"));
      } else {
        base::RecordAction(
            UserMetricsAction("SoundContentSetting.UnmuteBy.TabStrip"));
      }
      SetSitesMuted(GetIndicesForCommand(context_index), mute);
      break;
    }

    case CommandBookmarkAllTabs: {
      base::RecordAction(UserMetricsAction("TabContextMenu_BookmarkAllTabs"));

      delegate()->BookmarkAllTabs();
      break;
    }

    case CommandAddToNewGroup: {
      base::RecordAction(UserMetricsAction("TabContextMenu_AddToNewGroup"));

      AddToNewGroup(GetIndicesForCommand(context_index));
      break;
    }

    case CommandAddToExistingGroup: {
      // Do nothing. The submenu's delegate will invoke
      // ExecuteAddToExistingGroupCommand with the correct group later.
      break;
    }

    case CommandRemoveFromGroup: {
      base::RecordAction(UserMetricsAction("TabContextMenu_RemoveFromGroup"));
      RemoveFromGroup(GetIndicesForCommand(context_index));
      break;
    }

    default:
      NOTREACHED();
  }
}

void TabStripModel::ExecuteAddToExistingGroupCommand(int context_index,
                                                     TabGroupId group) {
  base::RecordAction(UserMetricsAction("TabContextMenu_AddToExistingGroup"));

  AddToExistingGroup(GetIndicesForCommand(context_index), group);
}

bool TabStripModel::WillContextMenuMuteSites(int index) {
  return !chrome::AreAllSitesMuted(*this, GetIndicesForCommand(index));
}

bool TabStripModel::WillContextMenuPin(int index) {
  std::vector<int> indices = GetIndicesForCommand(index);
  // If all tabs are pinned, then we unpin, otherwise we pin.
  bool all_pinned = true;
  for (size_t i = 0; i < indices.size() && all_pinned; ++i)
    all_pinned = IsTabPinned(indices[i]);
  return !all_pinned;
}

// static
bool TabStripModel::ContextMenuCommandToBrowserCommand(int cmd_id,
                                                       int* browser_cmd) {
  switch (cmd_id) {
    case CommandNewTab:
      *browser_cmd = IDC_NEW_TAB;
      break;
    case CommandReload:
      *browser_cmd = IDC_RELOAD;
      break;
    case CommandDuplicate:
      *browser_cmd = IDC_DUPLICATE_TAB;
      break;
    case CommandSendTabToSelf:
      *browser_cmd = IDC_SEND_TAB_TO_SELF;
      break;
    case CommandSendTabToSelfSingleTarget:
      *browser_cmd = IDC_SEND_TAB_TO_SELF_SINGLE_TARGET;
      break;
    case CommandCloseTab:
      *browser_cmd = IDC_CLOSE_TAB;
      break;
    case CommandRestoreTab:
      *browser_cmd = IDC_RESTORE_TAB;
      break;
    case CommandFocusMode:
      *browser_cmd = IDC_FOCUS_THIS_TAB;
      break;
    case CommandBookmarkAllTabs:
      *browser_cmd = IDC_BOOKMARK_ALL_TABS;
      break;
    default:
      *browser_cmd = 0;
      return false;
  }

  return true;
}

int TabStripModel::GetIndexOfNextWebContentsOpenedBy(const WebContents* opener,
                                                     int start_index) const {
  DCHECK(opener);
  DCHECK(ContainsIndex(start_index));

  // Check tabs after start_index first.
  for (int i = start_index + 1; i < count(); ++i) {
    if (contents_data_[i]->opener() == opener)
      return i;
  }
  // Then check tabs before start_index, iterating backwards.
  for (int i = start_index - 1; i >= 0; --i) {
    if (contents_data_[i]->opener() == opener)
      return i;
  }
  return kNoTab;
}

void TabStripModel::ForgetAllOpeners() {
  for (const auto& data : contents_data_)
    data->set_opener(nullptr);
}

void TabStripModel::ForgetOpener(WebContents* contents) {
  const int index = GetIndexOfWebContents(contents);
  DCHECK(ContainsIndex(index));
  contents_data_[index]->set_opener(nullptr);
}

bool TabStripModel::ShouldResetOpenerOnActiveTabChange(
    WebContents* contents) const {
  const int index = GetIndexOfWebContents(contents);
  DCHECK(ContainsIndex(index));
  return contents_data_[index]->reset_opener_on_active_tab_change();
}

///////////////////////////////////////////////////////////////////////////////
// TabStripModel, private:

bool TabStripModel::RunUnloadListenerBeforeClosing(
    content::WebContents* contents) {
  return delegate_->RunUnloadListenerBeforeClosing(contents);
}

bool TabStripModel::ShouldRunUnloadListenerBeforeClosing(
    content::WebContents* contents) {
  return contents->NeedToFireBeforeUnload() ||
         delegate_->ShouldRunUnloadListenerBeforeClosing(contents);
}

int TabStripModel::ConstrainInsertionIndex(int index, bool pinned_tab) {
  return pinned_tab
             ? std::min(std::max(0, index), IndexOfFirstNonPinnedTab())
             : std::min(count(), std::max(index, IndexOfFirstNonPinnedTab()));
}

std::vector<int> TabStripModel::GetIndicesForCommand(int index) const {
  if (!IsTabSelected(index)) {
    std::vector<int> indices;
    indices.push_back(index);
    return indices;
  }
  return selection_model_.selected_indices();
}

std::vector<int> TabStripModel::GetIndicesClosedByCommand(
    int index,
    ContextMenuCommand id) const {
  DCHECK(ContainsIndex(index));
  DCHECK(id == CommandCloseTabsToRight || id == CommandCloseOtherTabs);
  bool is_selected = IsTabSelected(index);
  int last_unclosed_tab = -1;
  if (id == CommandCloseTabsToRight) {
    last_unclosed_tab =
        is_selected ? selection_model_.selected_indices().back() : index;
  }

  // NOTE: callers expect the vector to be sorted in descending order.
  std::vector<int> indices;
  for (int i = count() - 1; i > last_unclosed_tab; --i) {
    if (i != index && !IsTabPinned(i) && (!is_selected || !IsTabSelected(i)))
      indices.push_back(i);
  }
  return indices;
}

bool TabStripModel::IsNewTabAtEndOfTabStrip(WebContents* contents) const {
  const GURL& url = contents->GetLastCommittedURL();
  return url.SchemeIs(content::kChromeUIScheme) &&
         url.host_piece() == chrome::kChromeUINewTabHost &&
         contents == GetWebContentsAtImpl(count() - 1) &&
         contents->GetController().GetEntryCount() == 1;
}

std::vector<content::WebContents*> TabStripModel::GetWebContentsesByIndices(
    const std::vector<int>& indices) {
  std::vector<content::WebContents*> items;
  items.reserve(indices.size());
  for (int index : indices)
    items.push_back(GetWebContentsAtImpl(index));
  return items;
}

int TabStripModel::InsertWebContentsAtImpl(
    int index,
    std::unique_ptr<content::WebContents> contents,
    int add_types,
    base::Optional<TabGroupId> group) {
  delegate()->WillAddWebContents(contents.get());

  bool active = (add_types & ADD_ACTIVE) != 0;
  bool pin = (add_types & ADD_PINNED) != 0;
  if (group.has_value())
    pin = IsGroupPinned(group.value());
  index = ConstrainInsertionIndex(index, pin);

  // Have to get the active contents before we monkey with the contents
  // otherwise we run into problems when we try to change the active contents
  // since the old contents and the new contents will be the same...
  WebContents* active_contents = GetActiveWebContents();
  WebContents* raw_contents = contents.get();
  std::unique_ptr<WebContentsData> data =
      std::make_unique<WebContentsData>(std::move(contents));
  data->set_pinned(pin);
  if ((add_types & ADD_INHERIT_OPENER) && active_contents) {
    if (active) {
      // Forget any existing relationships, we don't want to make things too
      // confusing by having multiple openers active at the same time.
      ForgetAllOpeners();
    }
    data->set_opener(active_contents);
  }
  data->set_group(group);
  if (group.has_value())
    group_data_[group.value()].TabAdded();

  // TODO(gbillock): Ask the modal dialog manager whether the WebContents should
  // be blocked, or just let the modal dialog manager make the blocking call
  // directly and not use this at all.
  const web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(raw_contents);
  if (manager)
    data->set_blocked(manager->IsDialogActive());

  TabStripSelectionChange selection(GetActiveWebContents(), selection_model_);

  contents_data_.insert(contents_data_.begin() + index, std::move(data));

  selection_model_.IncrementFrom(index);

  if (active) {
    ui::ListSelectionModel new_model = selection_model_;
    new_model.SetSelectedIndex(index);
    selection = SetSelection(std::move(new_model),
                             TabStripModelObserver::CHANGE_REASON_NONE,
                             /*triggered_by_other_operation=*/true);
  }

  TabStripModelChange::Insert insert;
  insert.contents.push_back({raw_contents, index});
  TabStripModelChange change(std::move(insert));
  for (auto& observer : observers_)
    observer.OnTabStripModelChanged(this, change, selection);
  if (group.has_value())
    NotifyGroupChange(index, base::nullopt, group);

  return index;
}

bool TabStripModel::InternalCloseTabs(
    base::span<content::WebContents* const> items,
    uint32_t close_types) {
  if (items.empty())
    return true;

  const bool closing_all = static_cast<int>(items.size()) == count();
  base::WeakPtr<TabStripModel> ref = weak_factory_.GetWeakPtr();
  if (closing_all) {
    for (auto& observer : observers_)
      observer.WillCloseAllTabs(this);
  }
  const bool closed_all = CloseWebContentses(items, close_types);
  if (!ref)
    return closed_all;
  if (closing_all) {
    // CloseAllTabsStopped is sent with reason kCloseAllCompleted if
    // closed_all; otherwise kCloseAllCanceled is sent.
    for (auto& observer : observers_)
      observer.CloseAllTabsStopped(
          this, closed_all ? TabStripModelObserver::kCloseAllCompleted
                           : TabStripModelObserver::kCloseAllCanceled);
  }

  return closed_all;
}

bool TabStripModel::CloseWebContentses(
    base::span<content::WebContents* const> items,
    uint32_t close_types) {
  if (items.empty())
    return true;

  // We only try the fast shutdown path if the whole browser process is *not*
  // shutting down. Fast shutdown during browser termination is handled in
  // browser_shutdown::OnShutdownStarting.
  if (browser_shutdown::GetShutdownType() == browser_shutdown::NOT_VALID) {
    // Construct a map of processes to the number of associated tabs that are
    // closing.
    base::flat_map<content::RenderProcessHost*, size_t> processes;
    for (content::WebContents* contents : items) {
      if (ShouldRunUnloadListenerBeforeClosing(contents))
        continue;
      content::RenderProcessHost* process =
          contents->GetMainFrame()->GetProcess();
      ++processes[process];
    }

    // Try to fast shutdown the tabs that can close.
    for (const auto& pair : processes)
      pair.first->FastShutdownIfPossible(pair.second, false);
  }

  DetachNotifications notifications(GetWebContentsAtImpl(active_index()),
                                    selection_model_, /*will_delete=*/true);

  // We now return to our regularly scheduled shutdown procedure.
  bool closed_all = true;

  // The indices of WebContents prior to any modification of the internal state.
  std::vector<int> original_indices;
  original_indices.resize(items.size());
  for (size_t i = 0; i < items.size(); ++i)
    original_indices[i] = GetIndexOfWebContents(items[i]);

  for (size_t i = 0; i < items.size(); ++i) {
    WebContents* closing_contents = items[i];

    // The index into contents_data_.
    int current_index = GetIndexOfWebContents(closing_contents);
    DCHECK_NE(current_index, kNoTab);

    // Update the explicitly closed state. If the unload handlers cancel the
    // close the state is reset in Browser. We don't update the explicitly
    // closed state if already marked as explicitly closed as unload handlers
    // call back to this if the close is allowed.
    if (!closing_contents->GetClosedByUserGesture()) {
      closing_contents->SetClosedByUserGesture(
          close_types & TabStripModel::CLOSE_USER_GESTURE);
    }

    if (RunUnloadListenerBeforeClosing(closing_contents)) {
      closed_all = false;
      continue;
    }

    std::unique_ptr<DetachedWebContents> dwc =
        std::make_unique<DetachedWebContents>(
            original_indices[i], current_index,
            DetachWebContentsImpl(current_index,
                                  close_types & CLOSE_CREATE_HISTORICAL_TAB,
                                  /*will_delete=*/true));
    notifications.detached_web_contents.push_back(std::move(dwc));
  }

  // When unload handler is triggered for all items, we should wait for the
  // result.
  if (!notifications.detached_web_contents.empty())
    SendDetachWebContentsNotifications(&notifications);

  return closed_all;
}

WebContents* TabStripModel::GetWebContentsAtImpl(int index) const {
  CHECK(ContainsIndex(index))
      << "Failed to find: " << index << " in: " << count() << " entries.";
  return contents_data_[index]->web_contents();
}


TabStripSelectionChange TabStripModel::SetSelection(
    ui::ListSelectionModel new_model,
    TabStripModelObserver::ChangeReason reason,
    bool triggered_by_other_operation) {
  TabStripSelectionChange selection;
  selection.old_model = selection_model_;
  selection.old_contents = GetActiveWebContents();
  selection.new_model = new_model;
  selection.reason = reason;

  // This is done after notifying TabDeactivated() because caller can assume
  // that TabStripModel::active_index() would return the index for
  // |selection.old_contents|.
  selection_model_ = new_model;
  selection.new_contents = GetActiveWebContents();

  if (!triggered_by_other_operation &&
      (selection.active_tab_changed() || selection.selection_changed())) {
    if (selection.active_tab_changed()) {
      auto now = base::TimeTicks::Now();
      if (selection.new_contents &&
          selection.new_contents->GetRenderWidgetHostView()) {
        auto input_event_timestamp =
            tab_switch_event_latency_recorder_.input_event_timestamp();
        // input_event_timestamp may be null in some cases, e.g. in tests.
        selection.new_contents->GetRenderWidgetHostView()
            ->SetRecordTabSwitchTimeRequest(
                !input_event_timestamp.is_null() ? input_event_timestamp : now,
                resource_coordinator::ResourceCoordinatorTabHelper::IsLoaded(
                    selection.new_contents),
                resource_coordinator::ResourceCoordinatorTabHelper::IsFrozen(
                    selection.new_contents));
      }
      tab_switch_event_latency_recorder_.OnWillChangeActiveTab(now);
    }
    TabStripModelChange change;
    auto visibility_tracker = InstallRenderWigetVisibilityTracker(selection);
    for (auto& observer : observers_)
      observer.OnTabStripModelChanged(this, change, selection);
  }

  return selection;
}

void TabStripModel::SelectRelativeTab(bool next, UserGestureDetails detail) {
  // This may happen during automated testing or if a user somehow buffers
  // many key accelerators.
  if (contents_data_.empty())
    return;

  int index = active_index();
  int delta = next ? 1 : -1;
  index = (index + count() + delta) % count();
  ActivateTabAt(index, detail);
}

void TabStripModel::MoveWebContentsAtImpl(int index,
                                          int to_position,
                                          bool select_after_move) {
  FixOpeners(index);

  TabStripSelectionChange selection(GetActiveWebContents(), selection_model_);

  std::unique_ptr<WebContentsData> moved_data =
      std::move(contents_data_[index]);
  WebContents* web_contents = moved_data->web_contents();
  contents_data_.erase(contents_data_.begin() + index);
  contents_data_.insert(contents_data_.begin() + to_position,
                        std::move(moved_data));

  selection_model_.Move(index, to_position, 1);
  if (!selection_model_.IsSelected(to_position) && select_after_move)
    selection_model_.SetSelectedIndex(to_position);
  selection.new_model = selection_model_;

  TabStripModelChange::Move move;
  move.contents = web_contents;
  move.from_index = index;
  move.to_index = to_position;
  TabStripModelChange change(move);
  for (auto& observer : observers_)
    observer.OnTabStripModelChanged(this, change, selection);
}

void TabStripModel::MoveSelectedTabsToImpl(int index,
                                           size_t start,
                                           size_t length) {
  DCHECK(start < selection_model_.selected_indices().size() &&
         start + length <= selection_model_.selected_indices().size());
  size_t end = start + length;
  int count_before_index = 0;
  for (size_t i = start; i < end && selection_model_.selected_indices()[i] <
                                        index + count_before_index;
       ++i) {
    count_before_index++;
  }

  // First move those before index. Any tabs before index end up moving in the
  // selection model so we use start each time through.
  int target_index = index + count_before_index;
  size_t tab_index = start;
  while (tab_index < end &&
         selection_model_.selected_indices()[start] < index) {
    MoveWebContentsAtImpl(selection_model_.selected_indices()[start],
                          target_index - 1, false);
    tab_index++;
  }

  // Then move those after the index. These don't result in reordering the
  // selection.
  while (tab_index < end) {
    if (selection_model_.selected_indices()[tab_index] != target_index) {
      MoveWebContentsAtImpl(selection_model_.selected_indices()[tab_index],
                            target_index, false);
    }
    tab_index++;
    target_index++;
  }
}

void TabStripModel::AddToNewGroupImpl(const std::vector<int>& indices,
                                      TabGroupId new_group) {
  DCHECK(!std::any_of(
      contents_data_.cbegin(), contents_data_.cend(),
      [new_group](const auto& datum) { return datum->group() == new_group; }));

  group_data_.emplace(new_group, TabGroupData());

  // Find a destination for the first tab that's not inside another group. We
  // will stack the rest of the tabs up to its right.
  int destination_index = -1;
  for (int i = indices[0]; i < count(); i++) {
    const int destination_candidate = i + 1;
    const bool end_of_strip = !ContainsIndex(destination_candidate);
    if (end_of_strip || !GetTabGroupForTab(destination_candidate).has_value() ||
        GetTabGroupForTab(destination_candidate) !=
            GetTabGroupForTab(indices[0])) {
      destination_index = destination_candidate;
      break;
    }
  }

  std::vector<int> new_indices = indices;
  if (IsTabPinned(new_indices[0]))
    new_indices = SetTabsPinned(new_indices, true);

  MoveTabsIntoGroup(new_indices, destination_index, new_group);
}

void TabStripModel::AddToExistingGroupImpl(const std::vector<int>& indices,
                                           TabGroupId group) {
  int destination_index = -1;
  bool pin = false;
  for (int i = contents_data_.size() - 1; i >= 0; i--) {
    if (contents_data_[i]->group() == group) {
      destination_index = i + 1;
      pin = IsTabPinned(i);
      break;
    }
  }

  // Ignore indices that are already in the group.
  std::vector<int> new_indices;
  for (int candidate_index : indices) {
    if (GetTabGroupForTab(candidate_index) != group)
      new_indices.push_back(candidate_index);
  }
  new_indices = SetTabsPinned(new_indices, pin);

  MoveTabsIntoGroup(new_indices, destination_index, group);
}

void TabStripModel::MoveTabsIntoGroup(const std::vector<int>& indices,
                                      int destination_index,
                                      TabGroupId group) {
  // Some tabs will need to be moved to the right, some to the left. We need to
  // handle those separately. First, move tabs to the right, starting with the
  // rightmost tab so we don't cause other tabs we are about to move to shift.
  int numTabsMovingRight = 0;
  for (size_t i = 0; i < indices.size() && indices[i] < destination_index;
       i++) {
    numTabsMovingRight++;
  }
  for (int i = numTabsMovingRight - 1; i >= 0; i--) {
    MoveAndSetGroup(indices[i], destination_index - numTabsMovingRight + i,
                    group);
  }

  // Collect indices for tabs moving to the left.
  std::vector<int> move_left_indices;
  for (size_t i = numTabsMovingRight; i < indices.size(); i++) {
    move_left_indices.push_back(indices[i]);
  }
  // Move tabs to the left, starting with the leftmost tab.
  for (size_t i = 0; i < move_left_indices.size(); i++)
    MoveAndSetGroup(move_left_indices[i], destination_index + i, group);
}

void TabStripModel::MoveAndSetGroup(int index,
                                    int new_index,
                                    base::Optional<TabGroupId> new_group) {
  base::Optional<TabGroupId> old_group = UngroupTab(index);

  // TODO(crbug.com/940677): Ideally the delta of type kGroupChanged below would
  // be batched with the move deltas resulting from MoveWebContentsAt, but that
  // is not possible right now.
  if (index != new_index)
    MoveWebContentsAtImpl(index, new_index, false);
  contents_data_[new_index]->set_group(new_group);
  if (new_group.has_value())
    group_data_[new_group.value()].TabAdded();

  NotifyGroupChange(new_index, old_group, new_group);
}

void TabStripModel::NotifyGroupChange(int index,
                                      base::Optional<TabGroupId> old_group,
                                      base::Optional<TabGroupId> new_group) {
  if (old_group == new_group)
    return;
  TabStripModelChange::GroupChange group_change;
  group_change.contents = GetWebContentsAt(index);
  group_change.index = index;
  group_change.old_group = old_group;
  group_change.new_group = new_group;
  TabStripModelChange change(group_change);
  TabStripSelectionChange selection(GetActiveWebContents(), selection_model_);
  for (auto& observer : observers_)
    observer.OnTabStripModelChanged(this, change, selection);
}

base::Optional<TabGroupId> TabStripModel::UngroupTab(int index) {
  base::Optional<TabGroupId> group = GetTabGroupForTab(index);
  if (!group.has_value())
    return base::nullopt;

  contents_data_[index]->set_group(base::nullopt);
  TabGroupData* group_data = &group_data_[group.value()];
  group_data->TabRemoved();
  // Delete the group if we just ungrouped the last tab in that group.
  if (group_data->empty())
    group_data_.erase(group.value());
  return group;
}

void TabStripModel::SetTabPinnedImpl(int index, bool pinned) {
  DCHECK(ContainsIndex(index));
  if (contents_data_[index]->pinned() == pinned)
    return;

  // The tab's position may have to change as the pinned tab state is changing.
  int non_pinned_tab_index = IndexOfFirstNonPinnedTab();
  contents_data_[index]->set_pinned(pinned);
  if (pinned && index != non_pinned_tab_index) {
    MoveWebContentsAtImpl(index, non_pinned_tab_index, false);
    index = non_pinned_tab_index;
  } else if (!pinned && index + 1 != non_pinned_tab_index) {
    MoveWebContentsAtImpl(index, non_pinned_tab_index - 1, false);
    index = non_pinned_tab_index - 1;
  }

  for (auto& observer : observers_) {
    observer.TabPinnedStateChanged(this, contents_data_[index]->web_contents(),
                                   index);
  }
}

std::vector<int> TabStripModel::SetTabsPinned(const std::vector<int>& indices,
                                              bool pinned) {
  std::vector<int> new_indices;
  if (pinned) {
    for (size_t i = 0; i < indices.size(); i++) {
      if (IsTabPinned(indices[i])) {
        new_indices.push_back(indices[i]);
      } else {
        SetTabPinnedImpl(indices[i], true);
        new_indices.push_back(IndexOfFirstNonPinnedTab() - 1);
      }
    }
  } else {
    for (size_t i = indices.size() - 1; i < indices.size(); i--) {
      if (!IsTabPinned(indices[i])) {
        new_indices.push_back(indices[i]);
      } else {
        SetTabPinnedImpl(indices[i], false);
        new_indices.push_back(IndexOfFirstNonPinnedTab());
      }
    }
    std::reverse(new_indices.begin(), new_indices.end());
  }
  return new_indices;
}

// Sets the sound content setting for each site at the |indices|.
void TabStripModel::SetSitesMuted(const std::vector<int>& indices,
                                  bool mute) const {
  for (int tab_index : indices) {
    content::WebContents* web_contents = GetWebContentsAt(tab_index);
    GURL url = web_contents->GetLastCommittedURL();
    if (url.SchemeIs(content::kChromeUIScheme)) {
      // chrome:// URLs don't have content settings but can be muted, so just
      // mute the WebContents.
      chrome::SetTabAudioMuted(web_contents, mute,
                               TabMutedReason::CONTENT_SETTING_CHROME,
                               std::string());
    } else {
      Profile* profile =
          Profile::FromBrowserContext(web_contents->GetBrowserContext());
      HostContentSettingsMap* settings =
          HostContentSettingsMapFactory::GetForProfile(profile);
      ContentSetting setting =
          mute ? CONTENT_SETTING_BLOCK : CONTENT_SETTING_ALLOW;
      if (setting == settings->GetDefaultContentSetting(
                         CONTENT_SETTINGS_TYPE_SOUND, nullptr)) {
        setting = CONTENT_SETTING_DEFAULT;
      }
      settings->SetContentSettingDefaultScope(
          url, url, CONTENT_SETTINGS_TYPE_SOUND, std::string(), setting);
    }
  }
}

void TabStripModel::FixOpeners(int index) {
  WebContents* old_contents = GetWebContentsAtImpl(index);
  for (auto& data : contents_data_) {
    if (data->opener() == old_contents)
      data->set_opener(contents_data_[index]->opener());
  }
}
