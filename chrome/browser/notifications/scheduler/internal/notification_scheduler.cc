// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/notification_scheduler.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/notifications/scheduler/internal/background_task_coordinator.h"
#include "chrome/browser/notifications/scheduler/internal/display_decider.h"
#include "chrome/browser/notifications/scheduler/internal/distribution_policy.h"
#include "chrome/browser/notifications/scheduler/internal/impression_history_tracker.h"
#include "chrome/browser/notifications/scheduler/internal/notification_entry.h"
#include "chrome/browser/notifications/scheduler/internal/notification_scheduler_context.h"
#include "chrome/browser/notifications/scheduler/internal/scheduled_notification_manager.h"
#include "chrome/browser/notifications/scheduler/internal/scheduler_utils.h"
#include "chrome/browser/notifications/scheduler/public/display_agent.h"
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
  InitHelper()
      : context_(nullptr),
        notification_manager_delegate_(nullptr),
        impression_tracker_delegate_(nullptr) {}

  ~InitHelper() = default;

  // Initializes subsystems in notification scheduler, |callback| will be
  // invoked if all initializations finished or anyone of them failed. The
  // object should be destroyed along with the |callback|.
  void Init(
      NotificationSchedulerContext* context,
      ScheduledNotificationManager::Delegate* notification_manager_delegate,
      ImpressionHistoryTracker::Delegate* impression_tracker_delegate,
      InitCallback callback) {
    // TODO(xingliu): Initialize the databases in parallel, we currently
    // initialize one by one to work around a shared db issue. See
    // https://crbug.com/978680.
    context_ = context;
    notification_manager_delegate_ = notification_manager_delegate;
    impression_tracker_delegate_ = impression_tracker_delegate;
    callback_ = std::move(callback);

    context_->impression_tracker()->Init(
        impression_tracker_delegate_,
        base::BindOnce(&InitHelper::OnImpressionTrackerInitialized,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  void OnImpressionTrackerInitialized(bool success) {
    if (!success) {
      std::move(callback_).Run(false /*success*/);
      return;
    }

    context_->notification_manager()->Init(
        notification_manager_delegate_,
        base::BindOnce(&InitHelper::OnNotificationManagerInitialized,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnNotificationManagerInitialized(bool success) {
    std::move(callback_).Run(success);
  }

  NotificationSchedulerContext* context_;
  ScheduledNotificationManager::Delegate* notification_manager_delegate_;
  ImpressionHistoryTracker::Delegate* impression_tracker_delegate_;
  InitCallback callback_;

  base::WeakPtrFactory<InitHelper> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(InitHelper);
};

// Implementation of NotificationScheduler.
class NotificationSchedulerImpl : public NotificationScheduler,
                                  public ScheduledNotificationManager::Delegate,
                                  public ImpressionHistoryTracker::Delegate {
 public:
  NotificationSchedulerImpl(
      std::unique_ptr<NotificationSchedulerContext> context)
      : context_(std::move(context)),
        task_start_time_(SchedulerTaskTime::kUnknown) {}

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
    ScheduleBackgroundTask();
  }

  void DeleteAllNotifications(SchedulerClientType type) override {
    context_->notification_manager()->DeleteNotifications(type);
  }

  void GetImpressionDetail(
      SchedulerClientType type,
      ImpressionDetail::ImpressionDetailCallback callback) override {
    context_->impression_tracker()->GetImpressionDetail(type,
                                                        std::move(callback));
  }

  void OnInitialized(std::unique_ptr<InitHelper>,
                     InitCallback init_callback,
                     bool success) {
    // TODO(xingliu): Tear down internal components if initialization failed.
    std::move(init_callback).Run(success);
    NotifyClientsAfterInit(success);
  }

  void NotifyClientsAfterInit(bool success) {
    std::vector<SchedulerClientType> clients;
    context_->client_registrar()->GetRegisteredClients(&clients);
    for (auto type : clients) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&NotificationSchedulerImpl::NotifyClientAfterInit,
                         weak_ptr_factory_.GetWeakPtr(), type, success));
    }
  }

  void NotifyClientAfterInit(SchedulerClientType type, bool success) {
    std::vector<const NotificationEntry*> notifications;
    context_->notification_manager()->GetNotifications(type, &notifications);
    std::set<std::string> guids;
    for (const auto* notification : notifications) {
      DCHECK(notification);
      guids.emplace(notification->guid);
    }

    auto* client = context_->client_registrar()->GetClient(type);
    DCHECK(client);
    client->OnSchedulerInitialized(success, std::move(guids));
  }

  // NotificationBackgroundTaskScheduler::Handler implementation.
  void OnStartTask(SchedulerTaskTime task_time,
                   TaskFinishedCallback callback) override {
    task_start_time_ = task_time;

    // Updates the impression data to compute daily notification shown budget.
    context_->impression_tracker()->AnalyzeImpressionHistory();

    // Show notifications.
    FindNotificationToShow(task_start_time_);

    // Schedule the next background task based on scheduled notifications.
    ScheduleBackgroundTask();

    std::move(callback).Run(false /*need_reschedule*/);
  }

  void OnStopTask(SchedulerTaskTime task_time) override {
    task_start_time_ = task_time;
    ScheduleBackgroundTask();
  }

  // ScheduledNotificationManager::Delegate implementation.
  void DisplayNotification(std::unique_ptr<NotificationEntry> entry) override {
    if (!entry) {
      DLOG(ERROR) << "Notification entry is null";
      return;
    }

    // Inform the client to update notification data.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&NotificationSchedulerImpl::NotifyClientBeforeDisplay,
                       weak_ptr_factory_.GetWeakPtr(), std::move(entry)));
  }

  void NotifyClientBeforeDisplay(std::unique_ptr<NotificationEntry> entry) {
    auto* client = context_->client_registrar()->GetClient(entry->type);
    if (!client)
      return;

    // Detach the notification data for client to rewrite.
    auto notification_data =
        std::make_unique<NotificationData>(std::move(entry->notification_data));
    client->BeforeShowNotification(
        std::move(notification_data),
        base::BindOnce(&NotificationSchedulerImpl::AfterClientUpdateData,
                       weak_ptr_factory_.GetWeakPtr(), std::move(entry)));
  }

  void AfterClientUpdateData(
      std::unique_ptr<NotificationEntry> entry,
      std::unique_ptr<NotificationData> updated_notification_data) {
    // Show the notification in UI.
    auto system_data = std::make_unique<DisplayAgent::SystemData>();
    system_data->type = entry->type;
    system_data->guid = entry->guid;
    context_->display_agent()->ShowNotification(
        std::move(updated_notification_data), std::move(system_data));

    // Tracks user impression on the notification to be shown.
    context_->impression_tracker()->AddImpression(entry->type, entry->guid);
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

    for (const auto& guid : results) {
      context_->notification_manager()->DisplayNotification(guid);
    }
  }

  void ScheduleBackgroundTask() {
    BackgroundTaskCoordinator::Notifications notifications;
    context_->notification_manager()->GetAllNotifications(&notifications);
    BackgroundTaskCoordinator::ClientStates client_states;
    context_->impression_tracker()->GetClientStates(&client_states);

    context_->background_task_coordinator()->ScheduleBackgroundTask(
        std::move(notifications), std::move(client_states), task_start_time_);
  }

  void OnClick(SchedulerClientType type, const std::string& guid) override {
    context_->impression_tracker()->OnClick(type, guid);

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&NotificationSchedulerImpl::NotifyClientAfterUserAction,
                       weak_ptr_factory_.GetWeakPtr(), UserActionType::kClick,
                       type, base::nullopt));
  }

  void OnActionClick(SchedulerClientType type,
                     const std::string& guid,
                     ActionButtonType button_type) override {
    context_->impression_tracker()->OnActionClick(type, guid, button_type);

    ButtonClickInfo button_info;
    // TODO(xingliu): Plumb the button id from platform.
    button_info.button_id = std::string();
    button_info.type = button_type;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&NotificationSchedulerImpl::NotifyClientAfterUserAction,
                       weak_ptr_factory_.GetWeakPtr(),
                       UserActionType::kButtonClick, type,
                       std::move(button_info)));
  }

  void OnDismiss(SchedulerClientType type, const std::string& guid) override {
    context_->impression_tracker()->OnDismiss(type, guid);

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&NotificationSchedulerImpl::NotifyClientAfterUserAction,
                       weak_ptr_factory_.GetWeakPtr(), UserActionType::kDismiss,
                       type, base::nullopt));
  }

  void NotifyClientAfterUserAction(
      UserActionType action_type,
      SchedulerClientType client_type,
      base::Optional<ButtonClickInfo> button_info) {
    auto* client = context_->client_registrar()->GetClient(client_type);
    if (!client)
      return;

    client->OnUserAction(action_type, std::move(button_info));
  }

  std::unique_ptr<NotificationSchedulerContext> context_;

  // The start time of the background task. SchedulerTaskTime::kUnknown if
  // currently not running in a background task.
  SchedulerTaskTime task_start_time_;

  base::WeakPtrFactory<NotificationSchedulerImpl> weak_ptr_factory_{this};
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
