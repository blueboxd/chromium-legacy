// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_manager.h"

#include <memory>

#include "base/bind.h"
#include "base/task_runner_util.h"
#include "base/time/default_clock.h"
#include "content/browser/conversions/conversion_storage_sql.h"

namespace content {

ConversionManager::ConversionManager(
    const base::FilePath& user_data_directory,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : storage_task_runner_(std::move(task_runner)),
      clock_(base::DefaultClock::GetInstance()),
      storage_(new ConversionStorageSql(user_data_directory, this, clock_),
               base::OnTaskRunnerDeleter(storage_task_runner_)),
      conversion_policy_(std::make_unique<ConversionPolicy>()),
      weak_factory_(this) {
  // Unretained is safe because any task to delete |storage_| will be posted
  // after this one.
  base::PostTaskAndReplyWithResult(
      storage_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ConversionStorage::Initialize,
                     base::Unretained(storage_.get())),
      base::BindOnce(&ConversionManager::OnInitCompleted,
                     weak_factory_.GetWeakPtr()));
}

ConversionManager::~ConversionManager() = default;

void ConversionManager::HandleConversion(const StorableConversion& conversion) {
  if (!storage_)
    return;

  // TODO(https://crbug.com/1043345): Add UMA for the number of conversions we
  // are logging to storage, and the number of new reports logged to storage.
  // Unretained is safe because any task to delete |storage_| will be posted
  // after this one.
  storage_task_runner_.get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(
              &ConversionStorage::MaybeCreateAndStoreConversionReports),
          base::Unretained(storage_.get()), conversion));
}

const ConversionPolicy& ConversionManager::GetConversionPolicy() const {
  return *conversion_policy_;
}

void ConversionManager::ProcessNewConversionReports(
    std::vector<ConversionReport>* reports) {
  for (ConversionReport& report : *reports) {
    report.report_time = conversion_policy_->GetReportTimeForConversion(report);
  }

  conversion_policy_->AssignAttributionCredits(reports);
}

int ConversionManager::GetMaxConversionsPerImpression() const {
  return conversion_policy_->GetMaxConversionsPerImpression();
}

void ConversionManager::OnInitCompleted(bool success) {
  if (!success)
    storage_.reset();
}

}  // namespace content
