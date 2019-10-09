// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/history/print_job_history_cleaner.h"

#include "base/bind.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/printing/history/print_job_database.h"
#include "chrome/browser/chromeos/printing/history/print_job_info.pb.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace {

constexpr base::TimeDelta kPrintJobHistoryUpdateInterval =
    base::TimeDelta::FromDays(1);

// This "PrintJobHistoryExpirationPeriod" policy value stands for storing the
// print job history indefinitely.
constexpr int kPrintJobHistoryIndefinite = -1;

// Returns true if |pref_service| has been initialized.
bool IsPrefServiceInitialized(PrefService* pref_service) {
  return pref_service->GetAllPrefStoresInitializationStatus() !=
         PrefService::INITIALIZATION_STATUS_WAITING;
}

bool IsPrintJobExpired(const printing::proto::PrintJobInfo& print_job_info,
                       base::Time now,
                       base::TimeDelta print_job_history_expiration_period) {
  base::Time completion_time =
      base::Time::FromJsTime(print_job_info.completion_time());
  return completion_time + print_job_history_expiration_period < now;
}

}  // namespace

PrintJobHistoryCleaner::PrintJobHistoryCleaner(
    PrintJobDatabase* print_job_database,
    PrefService* pref_service)
    : print_job_database_(print_job_database),
      pref_service_(pref_service),
      clock_(base::DefaultClock::GetInstance()),
      timer_(std::make_unique<base::RepeatingTimer>()) {}

PrintJobHistoryCleaner::~PrintJobHistoryCleaner() = default;

void PrintJobHistoryCleaner::Start() {
  timer_->Start(FROM_HERE, kPrintJobHistoryUpdateInterval, this,
                &PrintJobHistoryCleaner::CleanUp);
  CleanUp();
}

void PrintJobHistoryCleaner::CleanUp() {
  if (IsPrefServiceInitialized(pref_service_)) {
    OnPrefServiceInitialized(true);
    return;
  }
  // Register for a callback that will be invoked when |pref_service_| is
  // initialized.
  pref_service_->AddPrefInitObserver(
      base::BindOnce(&PrintJobHistoryCleaner::OnPrefServiceInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PrintJobHistoryCleaner::OverrideTimeForTesting(
    const base::Clock* clock,
    std::unique_ptr<base::RepeatingTimer> timer) {
  clock_ = clock;
  timer_ = std::move(timer);
}

void PrintJobHistoryCleaner::OnPrefServiceInitialized(bool success) {
  if (!success || !print_job_database_->IsInitialized() ||
      pref_service_->GetInteger(prefs::kPrintJobHistoryExpirationPeriod) ==
          kPrintJobHistoryIndefinite) {
    return;
  }
  print_job_database_->GetPrintJobs(
      base::BindOnce(&PrintJobHistoryCleaner::OnPrintJobsRetrieved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PrintJobHistoryCleaner::OnPrintJobsRetrieved(
    bool success,
    std::unique_ptr<std::vector<printing::proto::PrintJobInfo>>
        print_job_infos) {
  if (!success || !print_job_infos)
    return;
  std::vector<std::string> print_job_ids_to_remove;
  base::TimeDelta print_job_history_expiration_period =
      base::TimeDelta::FromDays(
          pref_service_->GetInteger(prefs::kPrintJobHistoryExpirationPeriod));
  for (const auto& print_job_info : *print_job_infos) {
    if (IsPrintJobExpired(print_job_info, clock_->Now(),
                          print_job_history_expiration_period)) {
      print_job_ids_to_remove.push_back(print_job_info.id());
    }
  }
  print_job_database_->DeletePrintJobs(print_job_ids_to_remove,
                                       base::DoNothing());
}

}  // namespace chromeos
