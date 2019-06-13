// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/scheduled_notification_manager.h"

#include <algorithm>
#include <map>

#include "base/bind.h"
#include "base/guid.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/scheduler/internal/notification_entry.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"

namespace notifications {
namespace {

// Comparator used to sort notification entries based on creation time.
bool CreateTimeCompare(const NotificationEntry* lhs,
                       const NotificationEntry* rhs) {
  DCHECK(lhs && rhs);
  return lhs->create_time <= rhs->create_time;
}

class ScheduledNotificationManagerImpl : public ScheduledNotificationManager {
 public:
  using Store = std::unique_ptr<CollectionStore<NotificationEntry>>;

  ScheduledNotificationManagerImpl(Store store)
      : store_(std::move(store)), delegate_(nullptr), weak_ptr_factory_(this) {}

 private:
  void Init(Delegate* delegate, InitCallback callback) override {
    DCHECK(!delegate_);
    delegate_ = delegate;
    store_->InitAndLoad(
        base::BindOnce(&ScheduledNotificationManagerImpl::OnStoreInitialized,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // NotificationManager implementation.
  void ScheduleNotification(
      std::unique_ptr<NotificationParams> notification_params) override {
    DCHECK(notification_params);
    std::string guid = notification_params->guid;
    DCHECK(!guid.empty());
    if (notifications_.find(guid) != notifications_.end()) {
      // TODO(xingliu): Report duplicate guid failure.
      return;
    }

    auto entry =
        std::make_unique<NotificationEntry>(notification_params->type, guid);
    entry->notification_data =
        std::move(notification_params->notification_data);
    entry->schedule_params = std::move(notification_params->schedule_params);
    auto* entry_ptr = entry.get();
    notifications_.emplace(guid, std::move(entry));
    store_->Add(
        guid, *entry_ptr,
        base::BindOnce(&ScheduledNotificationManagerImpl::OnNotificationAdded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void DisplayNotification(const std::string& guid) override {
    auto it = notifications_.find(guid);
    if (it == notifications_.end())
      return;

    // Move the entry to delegate, and delete it from the storage.
    auto notification_entry = std::move(it->second);
    notifications_.erase(guid);
    store_->Delete(
        guid,
        base::BindOnce(&ScheduledNotificationManagerImpl::OnNotificationDeleted,
                       weak_ptr_factory_.GetWeakPtr()));
    if (delegate_)
      delegate_->DisplayNotification(std::move(notification_entry));
  }

  void GetAllNotifications(Notifications* notifications) override {
    DCHECK(notifications);
    notifications->clear();
    for (const auto& pair : notifications_) {
      const auto& notif = pair.second;
      DCHECK(notif);
      (*notifications)[notif->type].emplace_back(notif.get());
    }

    // Sort by creation time for each notification type.
    for (auto it = notifications->begin(); it != notifications->end(); ++it) {
      std::sort(it->second.begin(), it->second.end(), &CreateTimeCompare);
    }
  }

  void DeleteNotifications(SchedulerClientType type) override {
    auto it = notifications_.begin();
    while (it != notifications_.end()) {
      const auto& entry = *it->second;
      ++it;
      if (entry.type == type) {
        store_->Delete(
            entry.guid,
            base::BindOnce(
                &ScheduledNotificationManagerImpl::OnNotificationDeleted,
                weak_ptr_factory_.GetWeakPtr()));
        notifications_.erase(entry.guid);
      }
    }
  }

  void OnStoreInitialized(InitCallback callback,
                          bool success,
                          CollectionStore<NotificationEntry>::Entries entries) {
    if (!success) {
      std::move(callback).Run(false);
      return;
    }

    for (auto it = entries.begin(); it != entries.end(); ++it) {
      std::string guid = (*it)->guid;
      notifications_.emplace(guid, std::move(*it));
    }

    std::move(callback).Run(true);
  }

  void OnNotificationAdded(bool success) { NOTIMPLEMENTED(); }

  void OnNotificationDeleted(bool success) { NOTIMPLEMENTED(); }

  Store store_;
  Delegate* delegate_;
  std::map<std::string, std::unique_ptr<NotificationEntry>> notifications_;

  base::WeakPtrFactory<ScheduledNotificationManagerImpl> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ScheduledNotificationManagerImpl);
};

}  // namespace

// static
std::unique_ptr<ScheduledNotificationManager>
ScheduledNotificationManager::Create(
    std::unique_ptr<CollectionStore<NotificationEntry>> store) {
  return std::make_unique<ScheduledNotificationManagerImpl>(std::move(store));
}

ScheduledNotificationManager::ScheduledNotificationManager() = default;

ScheduledNotificationManager::~ScheduledNotificationManager() = default;

}  // namespace notifications
