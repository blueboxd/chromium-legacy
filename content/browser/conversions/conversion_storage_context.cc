// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_storage_context.h"

#include "base/bind_helpers.h"
#include "content/browser/conversions/conversion_storage_sql.h"

namespace content {

ConversionStorageContext::ConversionStorageContext(
    scoped_refptr<base::SequencedTaskRunner> storage_task_runner,
    const base::FilePath& user_data_directory,
    std::unique_ptr<ConversionStorageDelegateImpl> delegate,
    const base::Clock* clock)
    : storage_task_runner_(std::move(storage_task_runner)),
      storage_(new ConversionStorageSql(user_data_directory,
                                        std::move(delegate),
                                        clock),
               base::OnTaskRunnerDeleter(storage_task_runner_)) {}

ConversionStorageContext::~ConversionStorageContext() = default;

void ConversionStorageContext::StoreImpression(
    const StorableImpression& impression) {
  // Unretained is safe when posting to |storage_task_runner_| because any task
  // to delete |storage_| will be posted after this one.
  storage_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ConversionStorage::StoreImpression,
                                base::Unretained(storage_.get()), impression));
}

void ConversionStorageContext::MaybeCreateAndStoreConversionReports(
    const StorableConversion& conversion,
    base::OnceCallback<void(int)> callback) {
  storage_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(
              &ConversionStorage::MaybeCreateAndStoreConversionReports),
          base::Unretained(storage_.get()), conversion));
}

void ConversionStorageContext::GetConversionsToReport(
    base::Time max_report_time,
    base::OnceCallback<void(std::vector<ConversionReport>)> callback) {
  storage_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ConversionStorage::GetConversionsToReport,
                     base::Unretained(storage_.get()), max_report_time),
      std::move(callback));
}

void ConversionStorageContext::GetActiveImpressions(
    base::OnceCallback<void(std::vector<StorableImpression>)> callback) {
  storage_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ConversionStorage::GetActiveImpressions,
                     base::Unretained(storage_.get())),
      std::move(callback));
}

void ConversionStorageContext::DeleteConversion(
    int64_t conversion_id,
    base::OnceCallback<void(bool)> callback) {
  storage_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ConversionStorage::DeleteConversion,
                     base::Unretained(storage_.get()), conversion_id),
      std::move(callback));
}

void ConversionStorageContext::ClearData(
    base::Time delete_begin,
    base::Time delete_end,
    base::RepeatingCallback<bool(const url::Origin&)> filter,
    base::OnceClosure callback) {
  storage_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&ConversionStorage::ClearData,
                     base::Unretained(storage_.get()), delete_begin, delete_end,
                     std::move(filter)),
      std::move(callback));
}

}  // namespace content
