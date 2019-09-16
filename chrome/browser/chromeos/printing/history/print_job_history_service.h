// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_HISTORY_PRINT_JOB_HISTORY_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_HISTORY_PRINT_JOB_HISTORY_SERVICE_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"
#include "chrome/browser/chromeos/printing/history/print_job_database.h"
#include "components/keyed_service/core/keyed_service.h"

namespace chromeos {

class CupsPrintJobManager;

// This service is responsible for maintaining print job history.
// It observes CupsPrintJobManager events and uses PrintJobDatabase as
// persistent storage for print job history.
class PrintJobHistoryService : public KeyedService,
                               public chromeos::CupsPrintJobManager::Observer {
 public:
  class Observer {
   public:
    virtual void OnPrintJobFinished(
        const printing::proto::PrintJobInfo& print_job_info) = 0;
  };

  PrintJobHistoryService(std::unique_ptr<PrintJobDatabase> print_job_database,
                         CupsPrintJobManager* print_job_manager);
  ~PrintJobHistoryService() override;

  // Retrieves all print jobs from the database.
  void GetPrintJobs(PrintJobDatabase::GetPrintJobsCallback callback);

  void AddObserver(PrintJobHistoryService::Observer* observer);
  void RemoveObserver(PrintJobHistoryService::Observer* observer);

 private:
  // CupsPrintJobManager::Observer
  void OnPrintJobDone(base::WeakPtr<CupsPrintJob> job) override;
  void OnPrintJobError(base::WeakPtr<CupsPrintJob> job) override;
  void OnPrintJobCancelled(base::WeakPtr<CupsPrintJob> job) override;

  void SavePrintJob(base::WeakPtr<CupsPrintJob> job);

  void OnPrintJobSaved(const printing::proto::PrintJobInfo& print_job_info,
                       bool success);

  std::unique_ptr<PrintJobDatabase> print_job_database_;
  CupsPrintJobManager* print_job_manager_;

  base::ObserverList<PrintJobHistoryService::Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(PrintJobHistoryService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_HISTORY_PRINT_JOB_HISTORY_SERVICE_H_
