// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_EXPORT_IMPORT_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_EXPORT_IMPORT_NOTIFICATION_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

class Profile;

namespace message_center {
class Notification;
}

namespace crostini {

class CrostiniExportImport;

enum class ExportImportType;

// Notification for Crostini export and import.
class CrostiniExportImportNotification
    : public message_center::NotificationObserver {
 public:
  enum class Status { RUNNING, DONE, FAILED };

  CrostiniExportImportNotification(Profile* profile,
                                   CrostiniExportImport* service,
                                   ExportImportType type,
                                   const std::string& notification_id,
                                   const base::FilePath& path);
  virtual ~CrostiniExportImportNotification();

  void UpdateStatus(Status status, int progress_percent);
  void set_message_failed(const base::string16& message) {
    message_failed_ = message;
  }

  // Getters for testing.
  Status get_status() { return status_; }
  message_center::Notification* get_notification() {
    return notification_.get();
  }

  // message_center::NotificationObserver:
  void Close(bool by_user) override;
  void Click(const base::Optional<int>& button_index,
             const base::Optional<base::string16>& reply) override;

 private:
  Profile* profile_;
  // These notifications are owned by the export service.
  CrostiniExportImport* service_;
  ExportImportType type_;
  base::FilePath path_;
  Status status_ = Status::RUNNING;
  // Time when the operation started.  Used for estimating time remaining.
  base::TimeTicks started_ = base::TimeTicks::Now();
  base::string16 title_running_;
  base::string16 title_done_;
  base::string16 message_done_;
  base::string16 title_failed_;
  base::string16 message_failed_;
  std::unique_ptr<message_center::Notification> notification_;
  bool closed_ = false;
  base::WeakPtrFactory<CrostiniExportImportNotification> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(CrostiniExportImportNotification);
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_EXPORT_IMPORT_NOTIFICATION_H_
