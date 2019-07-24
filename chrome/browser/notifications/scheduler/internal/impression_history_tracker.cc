// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/impression_history_tracker.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/numerics/ranges.h"
#include "chrome/browser/notifications/scheduler/internal/scheduler_utils.h"

namespace notifications {
namespace {

// Comparator used to sort notification entries based on creation time.
bool CreateTimeCompare(const Impression& lhs, const Impression& rhs) {
  return lhs.create_time < rhs.create_time;
}

std::string ToDatabaseKey(SchedulerClientType type) {
  switch (type) {
    case SchedulerClientType::kTest1:
      return "Test1";
    case SchedulerClientType::kTest2:
      return "Test2";
    case SchedulerClientType::kTest3:
      return "Test3";
    case SchedulerClientType::kUnknown:
      NOTREACHED();
      return std::string();
    case SchedulerClientType::kWebUI:
      return "WebUI";
  }
}

}  // namespace

ImpressionHistoryTrackerImpl::ImpressionHistoryTrackerImpl(
    const SchedulerConfig& config,
    std::vector<SchedulerClientType> registered_clients,
    std::unique_ptr<CollectionStore<ClientState>> store,
    base::Clock* clock)
    : store_(std::move(store)),
      config_(config),
      registered_clients_(std::move(registered_clients)),
      initialized_(false),
      delegate_(nullptr),
      clock_(clock) {}

ImpressionHistoryTrackerImpl::~ImpressionHistoryTrackerImpl() = default;

void ImpressionHistoryTrackerImpl::Init(Delegate* delegate,
                                        InitCallback callback) {
  DCHECK(!delegate_ && delegate && !initialized_);
  delegate_ = delegate;
  store_->InitAndLoad(
      base::BindOnce(&ImpressionHistoryTrackerImpl::OnStoreInitialized,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ImpressionHistoryTrackerImpl::AddImpression(SchedulerClientType type,
                                                 const std::string& guid) {
  DCHECK(initialized_);
  auto it = client_states_.find(type);
  if (it == client_states_.end())
    return;

  it->second->impressions.emplace_back(Impression(type, guid, clock_->Now()));
  impression_map_.emplace(guid, &it->second->impressions.back());
  SetNeedsUpdate(type, true /*needs_update*/);
  MaybeUpdateDb(type);
  NotifyImpressionUpdate();
}

void ImpressionHistoryTrackerImpl::AnalyzeImpressionHistory() {
  DCHECK(initialized_);
  for (auto& client_state : client_states_)
    AnalyzeImpressionHistory(client_state.second.get());
  if (MaybeUpdateAllDb())
    NotifyImpressionUpdate();
}

void ImpressionHistoryTrackerImpl::GetClientStates(
    std::map<SchedulerClientType, const ClientState*>* client_states) const {
  DCHECK(initialized_);
  DCHECK(client_states);
  client_states->clear();
  for (const auto& pair : client_states_) {
    client_states->emplace(pair.first, pair.second.get());
  }
}

void ImpressionHistoryTrackerImpl::GetImpressionDetail(
    SchedulerClientType type,
    ImpressionDetail::ImpressionDetailCallback callback) {
  DCHECK(initialized_);
  auto it = client_states_.find(type);
  if (it == client_states_.end())
    return;

  auto* state = it->second.get();
  int num_notification_shown_today =
      notifications::NotificationsShownToday(state);
  ImpressionDetail detail(state->current_max_daily_show,
                          num_notification_shown_today);
  std::move(callback).Run(std::move(detail));
}

void ImpressionHistoryTrackerImpl::OnClick(SchedulerClientType type,
                                           const std::string& guid) {
  OnClickInternal(guid, true /*update_db*/);
}

void ImpressionHistoryTrackerImpl::OnActionClick(SchedulerClientType type,
                                                 const std::string& guid,
                                                 ActionButtonType button_type) {
  OnButtonClickInternal(guid, button_type, true /*update_db*/);
}

void ImpressionHistoryTrackerImpl::OnDismiss(SchedulerClientType type,
                                             const std::string& guid) {
  OnDismissInternal(guid, true /*update_db*/);
}

void ImpressionHistoryTrackerImpl::OnStoreInitialized(
    InitCallback callback,
    bool success,
    CollectionStore<ClientState>::Entries entries) {
  if (!success) {
    std::move(callback).Run(false);
    return;
  }

  initialized_ = true;

  // Load the data to memory, and sort the impression list.
  for (auto it = entries.begin(); it != entries.end(); ++it) {
    auto& entry = (*it);
    auto type = entry->type;
    std::sort(entry->impressions.begin(), entry->impressions.end(),
              &CreateTimeCompare);
    for (auto& impression : entry->impressions) {
      impression_map_.emplace(impression.guid, &impression);
    }
    client_states_.emplace(type, std::move(*it));
  }

  SyncRegisteredClients();
  std::move(callback).Run(true);
}

void ImpressionHistoryTrackerImpl::SyncRegisteredClients() {
  // Remove deprecated clients.
  for (auto it = client_states_.begin(); it != client_states_.end();) {
    auto client_type = it->first;
    bool deprecated =
        std::find(registered_clients_.begin(), registered_clients_.end(),
                  client_type) == registered_clients_.end();
    if (deprecated) {
      store_->Delete(ToDatabaseKey(client_type), base::DoNothing());
      client_states_.erase(it++);
      continue;
    } else {
      it++;
    }
  }

  // Add new data for new registered client.
  for (const auto type : registered_clients_) {
    if (client_states_.find(type) == client_states_.end()) {
      auto new_client_data = CreateNewClientState(type, config_);

      DCHECK(new_client_data);
      store_->Add(ToDatabaseKey(type), *new_client_data.get(),
                  base::DoNothing());
      client_states_.emplace(type, std::move(new_client_data));
    }
  }
}

void ImpressionHistoryTrackerImpl::AnalyzeImpressionHistory(
    ClientState* client_state) {
  DCHECK(client_state);
  std::deque<Impression*> dismisses;
  base::Time now = clock_->Now();

  for (auto it = client_state->impressions.begin();
       it != client_state->impressions.end();) {
    auto* impression = &*it;

    // Prune out expired impression.
    if (now - impression->create_time > config_.impression_expiration) {
      impression_map_.erase(impression->guid);
      client_state->impressions.erase(it++);
      SetNeedsUpdate(client_state->type, true);
      continue;
    } else {
      ++it;
    }

    switch (impression->feedback) {
      case UserFeedback::kDismiss:
        dismisses.emplace_back(impression);
        PruneImpressionByCreateTime(
            &dismisses, impression->create_time - config_.dismiss_duration);

        // Three consecutive dismisses will result in suppression.
        CheckConsecutiveUserAction(client_state, &dismisses,
                                   config_.dismiss_count);
        break;
      case UserFeedback::kClick:
        OnClickInternal(impression->guid, false /*update_db*/);
        break;
      case UserFeedback::kHelpful:
        OnButtonClickInternal(impression->guid, ActionButtonType::kHelpful,
                              false /*update_db*/);
        break;
      case UserFeedback::kNotHelpful:
        OnButtonClickInternal(impression->guid, ActionButtonType::kUnhelpful,
                              false /*update_db*/);
        break;
      case UserFeedback::kIgnore:
        break;
      case UserFeedback::kNoFeedback:
        FALLTHROUGH;
      default:
        // The user didn't interact with the notification yet.
        continue;
        break;
    }
  }

  // Check suppression expiration.
  CheckSuppressionExpiration(client_state);
}

// static
void ImpressionHistoryTrackerImpl::PruneImpressionByCreateTime(
    std::deque<Impression*>* impressions,
    const base::Time& start_time) {
  DCHECK(impressions);
  while (!impressions->empty()) {
    if (impressions->front()->create_time > start_time)
      break;
    // Anything created before |start_time| is considered to have no effect and
    // will never be processed again.
    impressions->front()->integrated = true;
    impressions->pop_front();
  }
}

void ImpressionHistoryTrackerImpl::GenerateImpressionResult(
    Impression* impression) {
  DCHECK(impression);
  auto it = impression->impression_mapping.find(impression->feedback);
  if (it != impression->impression_mapping.end()) {
    // Use client defined impression mapping.
    impression->impression = it->second;
  } else {
    // Use default mapping from user feedback to impression result.
    switch (impression->feedback) {
      case UserFeedback::kClick:
      case UserFeedback::kHelpful:
        impression->impression = ImpressionResult::kPositive;
        break;
      case UserFeedback::kDismiss:
      case UserFeedback::kIgnore:
        impression->impression = ImpressionResult::kNeutral;
        break;
      case UserFeedback::kNotHelpful:
        impression->impression = ImpressionResult::kNegative;
        break;
      case UserFeedback::kNoFeedback:
        NOTREACHED();
        break;
    }
  }
}

void ImpressionHistoryTrackerImpl::UpdateThrottling(ClientState* client_state,
                                                    Impression* impression) {
  DCHECK(client_state);
  DCHECK(impression);

  // Affect the notification throttling.
  switch (impression->impression) {
    case ImpressionResult::kPositive:
      ApplyPositiveImpression(client_state, impression);
      break;
    case ImpressionResult::kNegative:
      ApplyNegativeImpression(client_state, impression);
      break;
    case ImpressionResult::kNeutral:
      break;
    case ImpressionResult::kInvalid:
      NOTREACHED();
      break;
  }
}

void ImpressionHistoryTrackerImpl::CheckConsecutiveUserAction(
    ClientState* client_state,
    std::deque<Impression*>* impressions,
    size_t num_actions) {
  if (impressions->size() < num_actions)
    return;

  // Suppress the notification if the user performed consecutive operations that
  // generates negative impressions.
  for (size_t i = 0, size = impressions->size(); i < size; ++i) {
    Impression* impression = (*impressions)[i];
    if (impression->integrated)
      continue;

    impression->integrated = true;
    SetNeedsUpdate(client_state->type, true);
    GenerateImpressionResult(impression);
  }
}

void ImpressionHistoryTrackerImpl::ApplyPositiveImpression(
    ClientState* client_state,
    Impression* impression) {
  DCHECK(impression);
  if (impression->integrated)
    return;

  DCHECK_EQ(impression->impression, ImpressionResult::kPositive);
  SetNeedsUpdate(client_state->type, true);
  impression->integrated = true;

  // A positive impression directly releases the suppression.
  if (client_state->suppression_info.has_value()) {
    client_state->current_max_daily_show =
        client_state->suppression_info->recover_goal;
    client_state->suppression_info.reset();
    return;
  }

  // Increase |current_max_daily_show| by 1.
  client_state->current_max_daily_show =
      base::ClampToRange(++client_state->current_max_daily_show, 0,
                         config_.max_daily_shown_per_type);
}

void ImpressionHistoryTrackerImpl::ApplyNegativeImpression(
    ClientState* client_state,
    Impression* impression) {
  if (impression->integrated)
    return;

  DCHECK_EQ(impression->impression, ImpressionResult::kNegative);
  SetNeedsUpdate(client_state->type, true);
  impression->integrated = true;

  // Suppress the notification, the user will not see this type of notification
  // for a while.
  SuppressionInfo supression_info(clock_->Now(), config_.suppression_duration);
  client_state->suppression_info = std::move(supression_info);
  client_state->current_max_daily_show = 0;
}

void ImpressionHistoryTrackerImpl::CheckSuppressionExpiration(
    ClientState* client_state) {
  // No suppression to recover from.
  if (!client_state->suppression_info.has_value())
    return;

  SuppressionInfo& suppression = client_state->suppression_info.value();
  base::Time now = clock_->Now();

  // Still in the suppression time window.
  if (now - suppression.last_trigger_time < suppression.duration)
    return;

  // Recover from suppression and increase |current_max_daily_show|.
  DCHECK_EQ(client_state->current_max_daily_show, 0);
  client_state->current_max_daily_show = suppression.recover_goal;

  // Clear suppression if fully recovered.
  client_state->suppression_info.reset();
  SetNeedsUpdate(client_state->type, true);
}

bool ImpressionHistoryTrackerImpl::MaybeUpdateDb(SchedulerClientType type) {
  auto it = client_states_.find(type);
  if (it == client_states_.end())
    return false;

  bool db_updated = false;
  if (NeedsUpdate(type)) {
    store_->Update(ToDatabaseKey(type), *(it->second.get()), base::DoNothing());
    db_updated = true;
  }
  SetNeedsUpdate(type, false);
  return db_updated;
}

bool ImpressionHistoryTrackerImpl::MaybeUpdateAllDb() {
  bool db_updated = false;
  for (const auto& client_state : client_states_) {
    auto type = client_state.second->type;
    db_updated |= MaybeUpdateDb(type);
  }

  return db_updated;
}

void ImpressionHistoryTrackerImpl::SetNeedsUpdate(SchedulerClientType type,
                                                  bool needs_update) {
  need_update_db_[type] = needs_update;
}

bool ImpressionHistoryTrackerImpl::NeedsUpdate(SchedulerClientType type) const {
  auto it = need_update_db_.find(type);
  return it == need_update_db_.end() ? false : it->second;
}

void ImpressionHistoryTrackerImpl::NotifyImpressionUpdate() {
  if (delegate_)
    delegate_->OnImpressionUpdated();
}

Impression* ImpressionHistoryTrackerImpl::FindImpressionNeedsUpdate(
    const std::string& notification_guid) {
  auto it = impression_map_.find(notification_guid);
  if (it == impression_map_.end())
    return nullptr;
  auto* impression = it->second;

  if (impression->integrated)
    return nullptr;

  return it->second;
}

void ImpressionHistoryTrackerImpl::OnClickInternal(
    const std::string& notification_guid,
    bool update_db) {
  auto* impression = FindImpressionNeedsUpdate(notification_guid);
  if (!impression)
    return;

  auto it = client_states_.find(impression->type);
  if (it == client_states_.end())
    return;
  ClientState* client_state = it->second.get();
  impression->feedback = UserFeedback::kClick;
  GenerateImpressionResult(impression);
  SetNeedsUpdate(impression->type, true);
  UpdateThrottling(client_state, impression);

  if (update_db && MaybeUpdateDb(client_state->type))
    NotifyImpressionUpdate();
}

void ImpressionHistoryTrackerImpl::OnButtonClickInternal(
    const std::string& notification_guid,
    ActionButtonType button_type,
    bool update_db) {
  auto* impression = FindImpressionNeedsUpdate(notification_guid);
  if (!impression)
    return;
  auto it = client_states_.find(impression->type);
  if (it == client_states_.end())
    return;

  ClientState* client_state = it->second.get();
  switch (button_type) {
    case ActionButtonType::kHelpful:
      impression->feedback = UserFeedback::kHelpful;
      break;
    case ActionButtonType::kUnhelpful:
      impression->feedback = UserFeedback::kNotHelpful;
      break;
    case ActionButtonType::kUnknownAction:
      NOTIMPLEMENTED();
      break;
  }

  GenerateImpressionResult(impression);
  SetNeedsUpdate(impression->type, true);
  UpdateThrottling(client_state, impression);

  if (update_db && MaybeUpdateDb(client_state->type))
    NotifyImpressionUpdate();
}

void ImpressionHistoryTrackerImpl::OnDismissInternal(
    const std::string& notification_guid,
    bool update_db) {
  auto* impression = FindImpressionNeedsUpdate(notification_guid);
  if (!impression)
    return;

  auto it = client_states_.find(impression->type);
  if (it == client_states_.end())
    return;
  ClientState* client_state = it->second.get();

  impression->feedback = UserFeedback::kDismiss;
  SetNeedsUpdate(impression->type, true);

  // Check consecutive dismisses.
  AnalyzeImpressionHistory(client_state);
  if (MaybeUpdateDb(impression->type))
    NotifyImpressionUpdate();
}

}  // namespace notifications
