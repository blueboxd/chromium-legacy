// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_EXPORT_IMPORT_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_EXPORT_IMPORT_NOTIFICATION_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/crostini/crostini_export_import_status_tracker.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

class Profile;

namespace message_center {
class Notification;
}

namespace crostini {

enum class ExportImportType;

// Notification for Crostini export and import.
class CrostiniExportImportNotification
    : public CrostiniExportImportStatusTracker,
      public message_center::NotificationObserver {
 public:
  // Used to construct CrostiniExportImportNotification to ensure it controls
  // its lifetime.
  static CrostiniExportImportStatusTracker* Create(
      Profile* profile,
      ContainerId container_id,
      const std::string& notification_id,
      ExportImportType type,
      base::FilePath path) {
    return new CrostiniExportImportNotification(profile, type, notification_id,
                                                std::move(path),
                                                std::move(container_id));
  }

  ~CrostiniExportImportNotification() override;

  // Getters for testing.
  message_center::Notification* get_notification() {
    return notification_.get();
  }

  // message_center::NotificationObserver:
  void Close(bool by_user) override;
  void Click(const base::Optional<int>& button_index,
             const base::Optional<base::string16>& reply) override;

 private:
  CrostiniExportImportNotification(Profile* profile,
                                   ExportImportType type,
                                   const std::string& notification_id,
                                   base::FilePath path,
                                   ContainerId container_id);

  // CrostiniExportImportStatusTracker:
  void ForceRedisplay() override;
  void SetStatusRunningUI(int progress_percent) override;
  void SetStatusCancellingUI() override;
  void SetStatusDoneUI() override;
  void SetStatusCancelledUI() override;
  void SetStatusFailedWithMessageUI(Status status,
                                    const base::string16& message) override;

  Profile* profile_;  // Not owned.
  ContainerId container_id_;

  // Time when the operation started.  Used for estimating time remaining.
  base::TimeTicks started_ = base::TimeTicks::Now();
  std::unique_ptr<message_center::Notification> notification_;
  bool hidden_ = false;
  base::WeakPtrFactory<CrostiniExportImportNotification> weak_ptr_factory_{
      this};
  DISALLOW_COPY_AND_ASSIGN(CrostiniExportImportNotification);
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_EXPORT_IMPORT_NOTIFICATION_H_
