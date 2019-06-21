// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/notification_scheduler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/notifications/scheduler/internal/background_task_coordinator.h"
#include "chrome/browser/notifications/scheduler/internal/display_decider.h"
#include "chrome/browser/notifications/scheduler/internal/distribution_policy.h"
#include "chrome/browser/notifications/scheduler/internal/icon_store.h"
#include "chrome/browser/notifications/scheduler/internal/impression_history_tracker.h"
#include "chrome/browser/notifications/scheduler/internal/notification_entry.h"
#include "chrome/browser/notifications/scheduler/internal/notification_scheduler_context.h"
#include "chrome/browser/notifications/scheduler/internal/scheduled_notification_manager.h"
#include "chrome/browser/notifications/scheduler/public/notification_background_task_scheduler.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client_registrar.h"
#include "chrome/browser/notifications/scheduler/public/user_action_handler.h"

namespace notifications {
namespace {

class NotificationSchedulerImpl;

// Helper class to do async initialization in parallel for multiple subsystem
// instances.
class InitHelper {
 public:
  using InitCallback = base::OnceCallback<void(bool)>;
  InitHelper() : weak_ptr_factory_(this) {}

  ~InitHelper() = default;

  // Initializes subsystems in notification scheduler, |callback| will be
  // invoked if all initializations finished or anyone of them failed. The
  // object should be destroyed along with the |callback|.
  void Init(
      NotificationSchedulerContext* context,
      ScheduledNotificationManager::Delegate* notification_manager_delegate,
      ImpressionHistoryTracker::Delegate* impression_tracker_delegate,
      InitCallback callback) {
    callback_ = std::move(callback);
    context->icon_store()->Init(base::BindOnce(
        &InitHelper::OnIconStoreInitialized, weak_ptr_factory_.GetWeakPtr()));
    context->impression_tracker()->Init(
        impression_tracker_delegate,
        base::BindOnce(&InitHelper::OnImpressionTrackerInitialized,
                       weak_ptr_factory_.GetWeakPtr()));
    context->notification_manager()->Init(
        notification_manager_delegate,
        base::BindOnce(&InitHelper::OnNotificationManagerInitialized,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  void OnIconStoreInitialized(bool success) {
    icon_store_initialzed_ = success;
    MaybeFinishInitialization();
  }

  void OnImpressionTrackerInitialized(bool success) {
    impression_tracker_initialzed_ = success;
    MaybeFinishInitialization();
  }

  void OnNotificationManagerInitialized(bool success) {
    notification_manager_initialized_ = success;
    MaybeFinishInitialization();
  }

  void MaybeFinishInitialization() {
    bool all_finished = icon_store_initialzed_.has_value() &&
                        impression_tracker_initialzed_.has_value() &&
                        notification_manager_initialized_.has_value();
    // Notify the initialization result when all subcomponents are initialized.
    if (!all_finished)
      return;

    bool success = icon_store_initialzed_.value() &&
                   impression_tracker_initialzed_.value() &&
                   notification_manager_initialized_.value();
    std::move(callback_).Run(success);
  }

  InitCallback callback_;
  base::Optional<bool> icon_store_initialzed_;
  base::Optional<bool> impression_tracker_initialzed_;
  base::Optional<bool> notification_manager_initialized_;

  base::WeakPtrFactory<InitHelper> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(InitHelper);
};

// Implementation of NotificationScheduler.
class NotificationSchedulerImpl : public NotificationScheduler,
                                  public ScheduledNotificationManager::Delegate,
                                  public ImpressionHistoryTracker::Delegate {
 public:
  NotificationSchedulerImpl(
      std::unique_ptr<NotificationSchedulerContext> context)
      : context_(std::move(context)), weak_ptr_factory_(this) {}

  ~NotificationSchedulerImpl() override = default;

 private:
  // NotificationScheduler implementation.
  void Init(InitCallback init_callback) override {
    auto helper = std::make_unique<InitHelper>();
    auto* helper_ptr = helper.get();
    helper_ptr->Init(
        context_.get(), this, this,
        base::BindOnce(&NotificationSchedulerImpl::OnInitialized,
                       weak_ptr_factory_.GetWeakPtr(), std::move(helper),
                       std::move(init_callback)));
  }

  void Schedule(
      std::unique_ptr<NotificationParams> notification_params) override {
    context_->notification_manager()->ScheduleNotification(
        std::move(notification_params));
  }

  void OnInitialized(std::unique_ptr<InitHelper>,
                     InitCallback init_callback,
                     bool success) {
    // TODO(xingliu): Inform the clients about initialization results and tear
    // down internal components.
    std::move(init_callback).Run(success);
  }

  // NotificationBackgroundTaskScheduler::Handler implementation.
  void OnStartTask() override {
    // Updates the impression data to compute daily notification shown budget.
    context_->impression_tracker()->AnalyzeImpressionHistory();

    // TODO(xingliu): Pass SchedulerTaskTime from background task.
    FindNotificationToShow(SchedulerTaskTime::kMorning);

    // Schedule the next background task based on scheduled notifications.
    ScheduleBackgroundTask();
  }

  void OnStopTask() override { ScheduleBackgroundTask(); }

  // ScheduledNotificationManager::Delegate implementation.
  void DisplayNotification(
      std::unique_ptr<NotificationEntry> notification) override {
    // TODO(xingliu): Inform the clients and show the notification.
    context_->impression_tracker()->AddImpression(notification->type,
                                                  notification->guid);
  }

  // ImpressionHistoryTracker::Delegate implementation.
  void OnImpressionUpdated() override { ScheduleBackgroundTask(); }

  void FindNotificationToShow(SchedulerTaskTime task_start_time) {
    DisplayDecider::Results results;
    ScheduledNotificationManager::Notifications notifications;
    context_->notification_manager()->GetAllNotifications(&notifications);

    DisplayDecider::ClientStates client_states;
    context_->impression_tracker()->GetClientStates(&client_states);

    std::vector<SchedulerClientType> clients;
    context_->client_registrar()->GetRegisteredClients(&clients);

    context_->display_decider()->FindNotificationsToShow(
        context_->config(), std::move(clients), DistributionPolicy::Create(),
        task_start_time, std::move(notifications), std::move(client_states),
        &results);

    // TODO(xingliu): Update impression data after notification shown.
    // See https://crbug.com/965133.
    for (const auto& guid : results) {
      context_->notification_manager()->DisplayNotification(guid);
    }
  }

  void ScheduleBackgroundTask() {
    BackgroundTaskCoordinator::Notifications notifications;
    context_->notification_manager()->GetAllNotifications(&notifications);
    BackgroundTaskCoordinator::ClientStates client_states;
    context_->impression_tracker()->GetClientStates(&client_states);

    // TODO(xingliu): Pass SchedulerTaskTime from background task.
    context_->background_task_coordinator()->ScheduleBackgroundTask(
        std::move(notifications), std::move(client_states),
        SchedulerTaskTime::kMorning);
  }

  void OnClick(const std::string& notification_id) override {
    context_->impression_tracker()->OnClick(notification_id);
  }

  void OnActionClick(const std::string& notification_id,
                     ActionButtonType button_type) override {
    context_->impression_tracker()->OnActionClick(notification_id, button_type);
  }

  void OnDismiss(const std::string& notification_id) override {
    context_->impression_tracker()->OnDismiss(notification_id);
  }

  std::unique_ptr<NotificationSchedulerContext> context_;

  base::WeakPtrFactory<NotificationSchedulerImpl> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(NotificationSchedulerImpl);
};

}  // namespace

// static
std::unique_ptr<NotificationScheduler> NotificationScheduler::Create(
    std::unique_ptr<NotificationSchedulerContext> context) {
  return std::make_unique<NotificationSchedulerImpl>(std::move(context));
}

NotificationScheduler::NotificationScheduler() = default;

NotificationScheduler::~NotificationScheduler() = default;

}  // namespace notifications
