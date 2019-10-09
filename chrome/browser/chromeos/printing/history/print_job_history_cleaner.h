// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_HISTORY_PRINT_JOB_HISTORY_CLEANER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_HISTORY_PRINT_JOB_HISTORY_CLEANER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/printing/history/print_job_database.h"

class PrefService;

namespace base {
class Clock;
class RepeatingTimer;
}  // namespace base

namespace chromeos {

class PrintJobDatabase;

class PrintJobHistoryCleaner {
 public:
  // The default amount of time the metadata of completed print job is stored on
  // the device.
  static constexpr int kDefaultPrintJobHistoryExpirationPeriodDays = 90;

  PrintJobHistoryCleaner(PrintJobDatabase* print_job_database,
                         PrefService* pref_service);
  ~PrintJobHistoryCleaner();

  void Start();
  void CleanUp();

  // Overrides elements responsible for time progression to allow testing.
  void OverrideTimeForTesting(const base::Clock* clock,
                              std::unique_ptr<base::RepeatingTimer> timer);

 private:
  void OnPrefServiceInitialized(bool success);
  void OnPrintJobsRetrieved(
      bool success,
      std::unique_ptr<std::vector<printing::proto::PrintJobInfo>>
          print_job_infos);

  // This object is owned by PrintJobHistoryService and outlives
  // PrintJobHistoryCleaner.
  PrintJobDatabase* print_job_database_;

  PrefService* pref_service_;

  // Points to the base::DefaultClock by default.
  const base::Clock* clock_;

  // Timer used to update |print_job_database_|.
  std::unique_ptr<base::RepeatingTimer> timer_;

  base::WeakPtrFactory<PrintJobHistoryCleaner> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PrintJobHistoryCleaner);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_HISTORY_PRINT_JOB_HISTORY_CLEANER_H_
