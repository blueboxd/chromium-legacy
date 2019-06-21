// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/scheduled_notification_manager.h"

#include <algorithm>
#include <map>
#include <unordered_set>

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

  ScheduledNotificationManagerImpl(
      Store store,
      const std::vector<SchedulerClientType>& clients)
      : store_(std::move(store)),
        clients_(clients.begin(), clients.end()),
        delegate_(nullptr),
        weak_ptr_factory_(this) {}

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
    auto type = notification_params->type;

    if (!clients_.count(type) ||
        (notifications_.count(type) && notifications_[type].count(guid))) {
      // TODO(xingliu): Report duplicate guid failure.
      return;
    }

    auto entry =
        std::make_unique<NotificationEntry>(notification_params->type, guid);
    entry->notification_data =
        std::move(notification_params->notification_data);
    entry->schedule_params = std::move(notification_params->schedule_params);
    auto* entry_ptr = entry.get();
    notifications_[type][guid] = std::move(entry);
    store_->Add(
        guid, *entry_ptr,
        base::BindOnce(&ScheduledNotificationManagerImpl::OnNotificationAdded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void DisplayNotification(const std::string& guid) override {
    std::unique_ptr<NotificationEntry> entry;
    for (auto it = notifications_.begin(); it != notifications_.end(); it++) {
      if (it->second.count(guid))
        entry = std::move(it->second[guid]);
    }
    // TODO(hesen): Inform delegate failure of finding the data.
    if (!entry)
      return;

    notifications_[entry->type].erase(entry->guid);
    if (notifications_[entry->type].empty())
      notifications_.erase(entry->type);

    store_->Delete(
        guid,
        base::BindOnce(&ScheduledNotificationManagerImpl::OnNotificationDeleted,
                       weak_ptr_factory_.GetWeakPtr()));

    if (delegate_)
      delegate_->DisplayNotification(std::move(entry));
  }

  void GetAllNotifications(Notifications* notifications) const override {
    DCHECK(notifications);
    notifications->clear();

    for (auto it = notifications_.begin(); it != notifications_.end(); it++) {
      auto type = it->first;
      for (const auto& pair : it->second) {
        (*notifications)[type].emplace_back(pair.second.get());
      }
    }

    // Sort by creation time for each notification type.
    for (auto it = notifications->begin(); it != notifications->end(); ++it) {
      std::sort(it->second.begin(), it->second.end(), &CreateTimeCompare);
    }
  }

  void GetNotifications(
      SchedulerClientType type,
      std::vector<const NotificationEntry*>* notifications) const override {
    DCHECK(notifications);
    notifications->clear();
    const auto it = notifications_.find(type);
    if (it == notifications_.end())
      return;
    for (const auto& pair : it->second)
      notifications->emplace_back(pair.second.get());
  }

  void DeleteNotifications(SchedulerClientType type) override {
    if (!notifications_.count(type))
      return;
    auto it = notifications_[type].begin();
    while (it != notifications_[type].end()) {
      const auto& entry = *it->second;
      ++it;
      store_->Delete(
          entry.guid,
          base::BindOnce(
              &ScheduledNotificationManagerImpl::OnNotificationDeleted,
              weak_ptr_factory_.GetWeakPtr()));
    }
    notifications_.erase(type);
  }

  // Sync with registered clients. Delete entrties in |notifications_| if
  // their clients are deprecated.
  void SyncRegisteredClients() {
    auto it = notifications_.begin();
    while (it != notifications_.end()) {
      auto type = it->first;
      it++;
      if (!clients_.count(type)) {
        DeleteNotifications(type);
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
      auto type = (*it)->type;
      if (clients_.count(type))
        notifications_[type].emplace(guid, std::move(*it));
    }
    SyncRegisteredClients();
    std::move(callback).Run(true);
  }

  void OnNotificationAdded(bool success) { NOTIMPLEMENTED(); }

  void OnNotificationDeleted(bool success) { NOTIMPLEMENTED(); }

  Store store_;
  const std::unordered_set<SchedulerClientType> clients_;
  Delegate* delegate_;
  std::map<SchedulerClientType,
           std::map<std::string, std::unique_ptr<NotificationEntry>>>
      notifications_;

  base::WeakPtrFactory<ScheduledNotificationManagerImpl> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ScheduledNotificationManagerImpl);
};
}  // namespace

// static
std::unique_ptr<ScheduledNotificationManager>
ScheduledNotificationManager::Create(
    std::unique_ptr<CollectionStore<NotificationEntry>> store,
    const std::vector<SchedulerClientType>& clients) {
  return std::make_unique<ScheduledNotificationManagerImpl>(std::move(store),
                                                            clients);
}

ScheduledNotificationManager::ScheduledNotificationManager() = default;

ScheduledNotificationManager::~ScheduledNotificationManager() = default;

}  // namespace notifications
