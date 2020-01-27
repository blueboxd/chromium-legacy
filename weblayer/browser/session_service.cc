// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/session_service.h"

#include <stddef.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/command_storage_manager.h"
#include "components/sessions/core/session_command.h"
#include "components/sessions/core/session_constants.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "weblayer/browser/browser_impl.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/tab_impl.h"

using sessions::ContentSerializedNavigationBuilder;
using sessions::SerializedNavigationEntry;

namespace weblayer {
namespace {

const SessionID& GetSessionIDForTab(Tab* tab) {
  sessions::SessionTabHelper* session_tab_helper =
      sessions::SessionTabHelper::FromWebContents(
          static_cast<TabImpl*>(tab)->web_contents());
  DCHECK(session_tab_helper);
  return session_tab_helper->session_id();
}

int GetIndexOfTab(BrowserImpl* browser, Tab* tab) {
  const std::vector<Tab*>& tabs = browser->GetTabs();
  auto iter = std::find(tabs.begin(), tabs.end(), tab);
  DCHECK(iter != tabs.end());
  return static_cast<int>(iter - tabs.begin());
}

}  // namespace

// Every kWritesPerReset commands triggers recreating the file.
constexpr int kWritesPerReset = 250;

// SessionService -------------------------------------------------------------

SessionService::SessionService(const base::FilePath& path, BrowserImpl* browser)
    : browser_(browser),
      browser_session_id_(SessionID::NewUnique()),
      command_storage_manager_(
          std::make_unique<sessions::CommandStorageManager>(path, this)),
      rebuild_on_next_save_(false) {
  browser_->AddObserver(this);
  command_storage_manager_->ScheduleGetCurrentSessionCommands(
      base::BindOnce(&SessionService::OnGotCurrentSessionCommands,
                     base::Unretained(this)),
      &cancelable_task_tracker_);
}

SessionService::~SessionService() {
  if (command_storage_manager_->HasPendingSave())
    command_storage_manager_->Save();
  browser_->RemoveObserver(this);
}

bool SessionService::ShouldUseDelayedSave() {
  return true;
}

void SessionService::OnWillSaveCommands() {
  if (!rebuild_on_next_save_)
    return;

  rebuild_on_next_save_ = false;
  command_storage_manager_->set_pending_reset(true);
  command_storage_manager_->ClearPendingCommands();
  tab_to_available_range_.clear();
  BuildCommandsForBrowser();
}

void SessionService::OnTabAdded(Tab* tab) {
  content::WebContents* web_contents =
      static_cast<TabImpl*>(tab)->web_contents();
  auto* tab_helper = sessions::SessionTabHelper::FromWebContents(web_contents);
  DCHECK(tab_helper);
  tab_helper->SetWindowID(browser_session_id_);

  // Record the association between the SessionStorageNamespace and the
  // tab.
  content::SessionStorageNamespace* session_storage_namespace =
      web_contents->GetController().GetDefaultSessionStorageNamespace();
  session_storage_namespace->SetShouldPersist(true);

  if (rebuild_on_next_save_)
    return;

  int index = GetIndexOfTab(browser_, tab);
  BuildCommandsForTab(static_cast<TabImpl*>(tab), index);
  const std::vector<Tab*>& tabs = browser_->GetTabs();
  for (int i = index + 1; i < static_cast<int>(tabs.size()); ++i) {
    ScheduleCommand(sessions::CreateSetTabIndexInWindowCommand(
        GetSessionIDForTab(tabs[i]), i));
  }
}

void SessionService::OnTabRemoved(Tab* tab, bool active_tab_changed) {
  // Allow the associated sessionStorage to get deleted; it won't be needed
  // in the session restore.
  content::WebContents* web_contents =
      static_cast<TabImpl*>(tab)->web_contents();
  content::SessionStorageNamespace* session_storage_namespace =
      web_contents->GetController().GetDefaultSessionStorageNamespace();
  session_storage_namespace->SetShouldPersist(false);

  if (rebuild_on_next_save_)
    return;

  ScheduleCommand(sessions::CreateTabClosedCommand(GetSessionIDForTab(tab)));
  const std::vector<Tab*>& tabs = browser_->GetTabs();
  for (size_t i = 0; i < tabs.size(); ++i) {
    ScheduleCommand(sessions::CreateSetTabIndexInWindowCommand(
        GetSessionIDForTab(tabs[i]), i));
  }
  auto i = tab_to_available_range_.find(GetSessionIDForTab(tab));
  if (i != tab_to_available_range_.end())
    tab_to_available_range_.erase(i);
}

void SessionService::OnActiveTabChanged(Tab* tab) {
  if (rebuild_on_next_save_)
    return;

  const int index = tab == nullptr ? -1 : GetIndexOfTab(browser_, tab);
  ScheduleCommand(sessions::CreateSetSelectedTabInWindowCommand(
      browser_session_id_, index));
}

void SessionService::SetTabUserAgentOverride(
    const SessionID& window_id,
    const SessionID& tab_id,
    const std::string& user_agent_override) {
  if (rebuild_on_next_save_)
    return;

  ScheduleCommand(sessions::CreateSetTabUserAgentOverrideCommand(
      tab_id, user_agent_override));
}

void SessionService::SetSelectedNavigationIndex(const SessionID& window_id,
                                                const SessionID& tab_id,
                                                int index) {
  if (rebuild_on_next_save_)
    return;

  if (tab_to_available_range_.find(tab_id) != tab_to_available_range_.end()) {
    if (index < tab_to_available_range_[tab_id].first ||
        index > tab_to_available_range_[tab_id].second) {
      // The new index is outside the range of what we've archived, schedule
      // a reset.
      ScheduleRebuildOnNextSave();
      return;
    }
  }
  ScheduleCommand(
      sessions::CreateSetSelectedNavigationIndexCommand(tab_id, index));
}

void SessionService::UpdateTabNavigation(
    const SessionID& window_id,
    const SessionID& tab_id,
    const SerializedNavigationEntry& navigation) {
  if (rebuild_on_next_save_)
    return;

  if (tab_to_available_range_.find(tab_id) != tab_to_available_range_.end()) {
    std::pair<int, int>& range = tab_to_available_range_[tab_id];
    range.first = std::min(navigation.index(), range.first);
    range.second = std::max(navigation.index(), range.second);
  }
  ScheduleCommand(CreateUpdateTabNavigationCommand(tab_id, navigation));
}

void SessionService::TabNavigationPathPruned(const SessionID& window_id,
                                             const SessionID& tab_id,
                                             int index,
                                             int count) {
  if (rebuild_on_next_save_)
    return;

  DCHECK_GE(index, 0);
  DCHECK_GT(count, 0);

  // Update the range of available indices.
  if (tab_to_available_range_.find(tab_id) != tab_to_available_range_.end()) {
    std::pair<int, int>& range = tab_to_available_range_[tab_id];

    // if both range.first and range.second are also deleted.
    if (range.second >= index && range.second < index + count &&
        range.first >= index && range.first < index + count) {
      range.first = range.second = 0;
    } else {
      // Update range.first
      if (range.first >= index + count)
        range.first = range.first - count;
      else if (range.first >= index && range.first < index + count)
        range.first = index;

      // Update range.second
      if (range.second >= index + count)
        range.second = std::max(range.first, range.second - count);
      else if (range.second >= index && range.second < index + count)
        range.second = std::max(range.first, index - 1);
    }
  }

  return ScheduleCommand(
      sessions::CreateTabNavigationPathPrunedCommand(tab_id, index, count));
}

void SessionService::TabNavigationPathEntriesDeleted(const SessionID& window_id,
                                                     const SessionID& tab_id) {
  if (rebuild_on_next_save_)
    return;

  // Multiple tabs might be affected by this deletion, so the rebuild is
  // delayed until next save.
  rebuild_on_next_save_ = true;
  command_storage_manager_->StartSaveTimer();
}

void SessionService::ScheduleRebuildOnNextSave() {
  rebuild_on_next_save_ = true;
  command_storage_manager_->StartSaveTimer();
}

void SessionService::OnGotCurrentSessionCommands(
    std::vector<std::unique_ptr<sessions::SessionCommand>> commands) {
  ScheduleRebuildOnNextSave();

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  SessionID active_window_id = SessionID::InvalidValue();
  sessions::RestoreSessionFromCommands(commands, &windows, &active_window_id);
  ProcessRestoreCommands(windows);

  if (browser_->GetTabs().empty()) {
    // Nothing to restore, or restore failed. Create a default tab.
    browser_->SetActiveTab(browser_->CreateTabForSessionRestore(nullptr));
  }
}

void SessionService::BuildCommandsForTab(TabImpl* tab, int index_in_browser) {
  DCHECK(tab);

  sessions::SessionTabHelper* session_tab_helper =
      sessions::SessionTabHelper::FromWebContents(tab->web_contents());
  DCHECK(session_tab_helper);
  const SessionID& session_id(session_tab_helper->session_id());
  command_storage_manager_->AppendRebuildCommand(
      sessions::CreateSetTabWindowCommand(browser_session_id_, session_id));

  content::NavigationController& controller =
      tab->web_contents()->GetController();
  const int current_index = controller.GetCurrentEntryIndex();
  const int min_index =
      std::max(current_index - sessions::gMaxPersistNavigationCount, 0);
  const int max_index =
      std::min(current_index + sessions::gMaxPersistNavigationCount,
               controller.GetEntryCount());
  const int pending_index = controller.GetPendingEntryIndex();
  tab_to_available_range_[session_id] =
      std::pair<int, int>(min_index, max_index);

  command_storage_manager_->AppendRebuildCommand(
      sessions::CreateLastActiveTimeCommand(
          session_id, tab->web_contents()->GetLastActiveTime()));

  const std::string& ua_override = tab->web_contents()->GetUserAgentOverride();
  if (!ua_override.empty()) {
    command_storage_manager_->AppendRebuildCommand(
        sessions::CreateSetTabUserAgentOverrideCommand(session_id,
                                                       ua_override));
  }

  for (int i = min_index; i < max_index; ++i) {
    content::NavigationEntry* entry = (i == pending_index)
                                          ? controller.GetPendingEntry()
                                          : controller.GetEntryAtIndex(i);
    DCHECK(entry);
    const SerializedNavigationEntry navigation =
        ContentSerializedNavigationBuilder::FromNavigationEntry(i, entry);
    command_storage_manager_->AppendRebuildCommand(
        CreateUpdateTabNavigationCommand(session_id, navigation));
  }
  command_storage_manager_->AppendRebuildCommand(
      sessions::CreateSetSelectedNavigationIndexCommand(session_id,
                                                        current_index));

  if (index_in_browser != -1) {
    command_storage_manager_->AppendRebuildCommand(
        sessions::CreateSetTabIndexInWindowCommand(session_id,
                                                   index_in_browser));
  }

  // Record the association between the sessionStorage namespace and the tab.
  content::SessionStorageNamespace* session_storage_namespace =
      controller.GetDefaultSessionStorageNamespace();
  ScheduleCommand(sessions::CreateSessionStorageAssociatedCommand(
      session_tab_helper->session_id(), session_storage_namespace->id()));
}

void SessionService::BuildCommandsForBrowser() {
  // This is necessary for SessionService to restore the browser. The type is
  // effectively ignored.
  command_storage_manager_->AppendRebuildCommand(
      sessions::CreateSetWindowTypeCommand(
          browser_session_id_,
          sessions::SessionWindow::WindowType::TYPE_NORMAL));

  int active_index = -1;
  int tab_index = 0;
  for (Tab* tab : browser_->GetTabs()) {
    BuildCommandsForTab(static_cast<TabImpl*>(tab), tab_index);
    if (tab == browser_->GetActiveTab())
      active_index = tab_index;
    ++tab_index;
  }

  command_storage_manager_->AppendRebuildCommand(
      sessions::CreateSetSelectedTabInWindowCommand(browser_session_id_,
                                                    active_index));
}

void SessionService::ScheduleCommand(
    std::unique_ptr<sessions::SessionCommand> command) {
  DCHECK(command);
  if (ReplacePendingCommand(command_storage_manager_.get(), &command))
    return;
  command_storage_manager_->ScheduleCommand(std::move(command));
  if (command_storage_manager_->commands_since_reset() >= kWritesPerReset)
    ScheduleRebuildOnNextSave();
}

void SessionService::ProcessRestoreCommands(
    const std::vector<std::unique_ptr<sessions::SessionWindow>>& windows) {
  if (windows.empty() || windows[0]->tabs.empty())
    return;

  const bool had_tabs = !browser_->GetTabs().empty();
  content::BrowserContext* browser_context =
      browser_->profile()->GetBrowserContext();
  for (int i = 0; i < static_cast<int>(windows[0]->tabs.size()); ++i) {
    const sessions::SessionTab& session_tab = *(windows[0]->tabs[i]);
    if (session_tab.navigations.empty())
      continue;

    // Associate sessionStorage (if any) to the restored tab.
    scoped_refptr<content::SessionStorageNamespace> session_storage_namespace;
    if (!session_tab.session_storage_persistent_id.empty()) {
      session_storage_namespace =
          content::BrowserContext::GetDefaultStoragePartition(browser_context)
              ->GetDOMStorageContext()
              ->RecreateSessionStorage(
                  session_tab.session_storage_persistent_id);
    }

    const int selected_navigation_index =
        session_tab.normalized_navigation_index();

    GURL restore_url =
        session_tab.navigations[selected_navigation_index].virtual_url();
    content::SessionStorageNamespaceMap session_storage_namespace_map;
    session_storage_namespace_map[std::string()] = session_storage_namespace;
    content::BrowserURLHandler::GetInstance()->RewriteURLIfNecessary(
        &restore_url, browser_context);
    content::WebContents::CreateParams create_params(
        browser_context,
        content::SiteInstance::ShouldAssignSiteForURL(restore_url)
            ? content::SiteInstance::CreateForURL(browser_context, restore_url)
            : nullptr);
    create_params.initially_hidden = true;
    create_params.desired_renderer_state =
        content::WebContents::CreateParams::kNoRendererProcess;
    create_params.last_active_time = session_tab.last_active_time;
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContents::CreateWithSessionStorage(
            create_params, session_storage_namespace_map);
    std::vector<std::unique_ptr<content::NavigationEntry>> entries =
        ContentSerializedNavigationBuilder::ToNavigationEntries(
            session_tab.navigations, browser_context);
    web_contents->SetUserAgentOverride(session_tab.user_agent_override, false);
    // CURRENT_SESSION matches what clank does. On the desktop, we should
    // use a different type.
    web_contents->GetController().Restore(selected_navigation_index,
                                          content::RestoreType::CURRENT_SESSION,
                                          &entries);
    DCHECK_EQ(0u, entries.size());
    TabImpl* tab =
        browser_->CreateTabForSessionRestore(std::move(web_contents));

    if (!had_tabs && i == (windows[0])->selected_tab_index)
      browser_->SetActiveTab(tab);
  }
  if (!had_tabs && !browser_->GetTabs().empty() && !browser_->GetActiveTab())
    browser_->SetActiveTab(browser_->GetTabs().back());
}

}  // namespace weblayer
