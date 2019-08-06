// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/display_decider.h"

#include <algorithm>

#include "chrome/browser/notifications/scheduler/internal/distribution_policy.h"
#include "chrome/browser/notifications/scheduler/internal/impression_types.h"
#include "chrome/browser/notifications/scheduler/internal/notification_entry.h"
#include "chrome/browser/notifications/scheduler/internal/scheduler_config.h"
#include "chrome/browser/notifications/scheduler/internal/scheduler_utils.h"

using Notifications = notifications::DisplayDecider::Notifications;
using Results = notifications::DisplayDecider::Results;
using ClientStates = notifications::DisplayDecider::ClientStates;

namespace notifications {
namespace {

// Helper class contains the actual logic to decide which notifications to show.
// This is an one-shot class, callers should create a new object each time.
class DecisionHelper {
 public:
  DecisionHelper(const SchedulerConfig* config,
                 const std::vector<SchedulerClientType>& clients,
                 DistributionPolicy* distribution_policy,
                 SchedulerTaskTime task_start_time,
                 Notifications notifications,
                 ClientStates client_states)
      : notifications_(std::move(notifications)),
        current_task_start_time_(task_start_time),
        client_states_(std::move(client_states)),
        config_(config),
        clients_(clients),
        policy_(distribution_policy),
        daily_max_to_show_all_types_(0),
        last_shown_type_(SchedulerClientType::kUnknown),
        shown_(0) {}

  ~DecisionHelper() = default;

  // Figures out a list of notifications to show.
  void DecideNotificationToShow(Results* results) {
    ComputeDailyQuotaAllTypes();
    CountNotificationsShownToday();
    PickNotificationToShow(results);
  }

 private:
  void ComputeDailyQuotaAllTypes() {
    // Only background task launch needs to comply to |policy_|.
    if (current_task_start_time_ == SchedulerTaskTime::kUnknown) {
      daily_max_to_show_all_types_ = config_->max_daily_shown_all_type;
      return;
    }

    int quota = std::max(config_->max_daily_shown_all_type - shown_, 0);
    DCHECK(policy_);
    daily_max_to_show_all_types_ =
        std::min(config_->max_daily_shown_all_type,
                 policy_->MaxToShow(current_task_start_time_, quota));
  }

  void CountNotificationsShownToday() {
    for (const auto& pair : client_states_) {
      auto type = pair.first;
      // TODO(xingliu): Ensure deprecated clients will not have data in storage.
      DCHECK(std::find(clients_.begin(), clients_.end(), type) !=
             clients_.end());
    }

    NotificationsShownToday(client_states_, &shown_per_type_, &shown_,
                            &last_shown_type_);
  }

  // Picks a list of notifications to show.
  void PickNotificationToShow(Results* to_show) {
    DCHECK(to_show);
    if (shown_ > config_->max_daily_shown_all_type || clients_.empty())
      return;

    // No previous shown notification, move the iterator to last element.
    // We will iterate through all client types later.
    auto it = std::find(clients_.begin(), clients_.end(), last_shown_type_);
    if (it == clients_.end()) {
      DCHECK_EQ(last_shown_type_, SchedulerClientType::kUnknown);
      last_shown_type_ = clients_.back();
      it = clients_.end() - 1;
    }

    DCHECK_NE(last_shown_type_, SchedulerClientType::kUnknown);
    size_t steps = 0u;

    // Circling around all clients to find new notification to show.
    // TODO(xingliu): Apply scheduling parameters here.
    do {
      // Move the iterator to next client type.
      DCHECK(it != clients_.end());
      if (++it == clients_.end())
        it = clients_.begin();
      ++steps;

      SchedulerClientType type = *it;

      // Check quota for all types and current background task type.
      if (ReachDailyQuota())
        break;

      // Check quota for this type, and continue to iterate other types.
      if (NoMoreNotificationToShow(type))
        continue;

      // Show the last notification in the vector. Notice the order depends on
      // how the vector is sorted.
      to_show->emplace(notifications_[type].back()->guid);
      notifications_[type].pop_back();
      shown_per_type_[type]++;
      shown_++;
      steps = 0u;

      // Stop if we didn't find anything new to show, and have looped around
      // all clients.
    } while (steps <= clients_.size());
  }

  bool NoMoreNotificationToShow(SchedulerClientType type) {
    auto it = client_states_.find(type);
    int max_daily_show =
        it == client_states_.end() ? 0 : it->second->current_max_daily_show;

    return notifications_[type].empty() ||
           shown_per_type_[type] >= config_->max_daily_shown_per_type ||
           shown_per_type_[type] >= max_daily_show;
  }

  bool ReachDailyQuota() const {
    return shown_ >= daily_max_to_show_all_types_;
  }

  // Scheduled notifications as candidates to display to the user.
  Notifications notifications_;

  const SchedulerTaskTime current_task_start_time_;
  const ClientStates client_states_;
  const SchedulerConfig* config_;
  const std::vector<SchedulerClientType> clients_;
  DistributionPolicy* policy_;
  int daily_max_to_show_all_types_;

  SchedulerClientType last_shown_type_;
  std::map<SchedulerClientType, int> shown_per_type_;
  int shown_;

  DISALLOW_COPY_AND_ASSIGN(DecisionHelper);
};

class DisplayDeciderImpl : public DisplayDecider {
 public:
  DisplayDeciderImpl(const SchedulerConfig* config,
                     std::vector<SchedulerClientType> clients,
                     std::unique_ptr<DistributionPolicy> distribution_policy)
      : config_(config),
        clients_(std::move(clients)),
        distribution_policy_(std::move(distribution_policy)) {}
  ~DisplayDeciderImpl() override = default;

 private:
  // DisplayDecider implementation.
  void FindNotificationsToShow(
      SchedulerTaskTime task_start_time,
      Notifications notifications,
      ClientStates client_states,
      Results* results) override {
    auto helper = std::make_unique<DecisionHelper>(
        config_, clients_, distribution_policy_.get(), task_start_time,
        std::move(notifications), std::move(client_states));
    helper->DecideNotificationToShow(results);
  }

  const SchedulerConfig* config_;
  const std::vector<SchedulerClientType> clients_;
  std::unique_ptr<DistributionPolicy> distribution_policy_;

  DISALLOW_COPY_AND_ASSIGN(DisplayDeciderImpl);
};

}  // namespace

// static
std::unique_ptr<DisplayDecider> DisplayDecider::Create(
    const SchedulerConfig* config,
    std::vector<SchedulerClientType> clients,
    std::unique_ptr<DistributionPolicy> distribution_policy) {
  return std::make_unique<DisplayDeciderImpl>(config, std::move(clients),
                                              std::move(distribution_policy));
}

}  // namespace notifications
