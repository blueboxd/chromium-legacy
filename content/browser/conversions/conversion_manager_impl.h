// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_MANAGER_IMPL_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_MANAGER_IMPL_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "content/browser/conversions/conversion_manager.h"
#include "content/browser/conversions/conversion_policy.h"
#include "content/browser/conversions/conversion_storage.h"

namespace base {
class Clock;
}  // namespace base

namespace content {

// Frequency we pull ConversionReports from storage and queue them to be
// reported.
extern CONTENT_EXPORT const base::TimeDelta
    kConversionManagerQueueReportsInterval;

class ConversionStorage;
class StoragePartition;

// Provides access to the manager owned by the default StoragePartition.
class ConversionManagerProviderImpl : public ConversionManager::Provider {
 public:
  ConversionManagerProviderImpl() = default;
  ConversionManagerProviderImpl(const ConversionManagerProviderImpl& other) =
      delete;
  ConversionManagerProviderImpl& operator=(
      const ConversionManagerProviderImpl& other) = delete;
  ~ConversionManagerProviderImpl() override = default;

  // ConversionManagerProvider:
  ConversionManager* GetManager(WebContents* web_contents) const override;
};

// UI thread class that manages the lifetime of the underlying conversion
// storage. Owned by the storage partition.
class CONTENT_EXPORT ConversionManagerImpl : public ConversionManager {
 public:
  // Interface which manages the ownership, queuing, and sending of pending
  // conversion reports. Owned by |this|.
  class ConversionReporter {
   public:
    virtual ~ConversionReporter() = default;

    // Adds |reports| to a shared queue of reports that need to be sent. The
    // reporter needs to notify it's owning manager when a report has been sent
    // via ConversionManager::HandleSentReport().
    virtual void AddReportsToQueue(std::vector<ConversionReport> reports) = 0;
  };

  static std::unique_ptr<ConversionManagerImpl> CreateForTesting(
      std::unique_ptr<ConversionReporter> reporter,
      std::unique_ptr<ConversionPolicy> policy,
      const base::Clock* clock,
      const base::FilePath& user_data_directory,
      scoped_refptr<base::SequencedTaskRunner> storage_task_runner);

  // |storage_task_runner| should run with base::TaskPriority::BEST_EFFORT.
  ConversionManagerImpl(
      StoragePartition* storage_partition,
      const base::FilePath& user_data_directory,
      scoped_refptr<base::SequencedTaskRunner> storage_task_runner);
  ConversionManagerImpl(const ConversionManagerImpl& other) = delete;
  ConversionManagerImpl& operator=(const ConversionManagerImpl& other) = delete;
  ~ConversionManagerImpl() override;

  // ConversionManager:
  void HandleImpression(const StorableImpression& impression) override;
  void HandleConversion(const StorableConversion& conversion) override;
  void HandleSentReport(int64_t conversion_id) override;
  void GetActiveImpressionsForWebUI(
      base::OnceCallback<void(std::vector<StorableImpression>)> callback)
      override;
  void GetReportsForWebUI(
      base::OnceCallback<void(std::vector<ConversionReport>)> callback,
      base::Time max_report_time) override;
  const ConversionPolicy& GetConversionPolicy() const override;
  void ClearData(base::Time delete_begin,
                 base::Time delete_end,
                 base::RepeatingCallback<bool(const url::Origin&)> filter,
                 base::OnceClosure done) override;

 private:
  ConversionManagerImpl(
      std::unique_ptr<ConversionReporter> reporter,
      std::unique_ptr<ConversionPolicy> policy,
      const base::Clock* clock,
      const base::FilePath& user_data_directory,
      scoped_refptr<base::SequencedTaskRunner> storage_task_runner);

  void OnInitCompleted(bool success);

  using ReportsHandlerFunc =
      base::OnceCallback<void(std::vector<ConversionReport>)>;
  void GetAndHandleReports(ReportsHandlerFunc handler_function);

  // Get the next set of reports from storage that need to be sent before the
  // next call from |get_and_queue_reports_timer_|. Adds the reports to
  // |reporter|.
  void GetAndQueueReportsForNextInterval();

  // Queue the given |reports| on |reporter_|.
  void QueueReports(std::vector<ConversionReport> reports);

  void HandleReportsExpiredAtStartup(std::vector<ConversionReport> reports);

  // Friend to expose the ConversionStorage and task runner, consider changing
  // to just expose the storage if it moves to SequenceBound.
  friend std::vector<ConversionReport> GetConversionsToReportForTesting(
      ConversionManagerImpl* manager,
      base::Time max_report_time);

  // Task runner used to perform operations on |storage_|. Runs with
  // base::TaskPriority::BEST_EFFORT.
  scoped_refptr<base::SequencedTaskRunner> storage_task_runner_;

  const base::Clock* clock_;

  // Timer which administers calls to GetAndQueueReportsForNextInterval().
  base::RepeatingTimer get_and_queue_reports_timer_;

  // Handle keeping track of conversion reports to send. Reports are fetched
  // from |storage_| and added to |reporter_| by |get_reports_timer_|.
  std::unique_ptr<ConversionReporter> reporter_;

  // ConversionStorage instance which is scoped to lifetime of
  // |storage_task_runner_|. |storage_| should be accessed by calling
  // base::PostTask with |storage_task_runner_|, and should not be accessed
  // directly. |storage_| can be null if the storage initialization did not
  // succeed.
  //
  // TODO(https://crbug.com/1066920): This should use base::SequenceBound to
  // avoid having to call PostTask manually, as well as use base::Unretained on
  // |storage_|.
  std::unique_ptr<ConversionStorage, base::OnTaskRunnerDeleter> storage_;

  // Policy used for controlling API configurations such as reporting and
  // attribution models. Unique ptr so it can be overridden for testing.
  std::unique_ptr<ConversionPolicy> conversion_policy_;

  base::WeakPtrFactory<ConversionManagerImpl> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_MANAGER_IMPL_H_
