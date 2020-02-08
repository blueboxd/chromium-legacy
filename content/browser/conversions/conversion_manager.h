// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_MANAGER_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_MANAGER_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "content/browser/conversions/conversion_policy.h"
#include "content/browser/conversions/conversion_storage.h"
#include "content/browser/conversions/storable_conversion.h"

namespace base {
class Clock;
}  // namespace base

namespace content {

class ConversionStorage;

// UI thread class that manages the lifetime of the underlying conversion
// storage. Owned by the storage partition.
class ConversionManager : public ConversionStorage::Delegate {
 public:
  // |storage_task_runner| should run with base::TaskPriority::BEST_EFFORT.
  ConversionManager(
      const base::FilePath& user_data_directory,
      scoped_refptr<base::SequencedTaskRunner> storage_task_runner);
  ConversionManager(const ConversionManager& other) = delete;
  ConversionManager& operator=(const ConversionManager& other) = delete;
  ~ConversionManager() override;

  // Process a newly registered conversion. Will create and log any new
  // conversion reports to storage.
  void HandleConversion(const StorableConversion& conversion);

  const ConversionPolicy& GetConversionPolicy() const;

 private:
  // ConversionStorageDelegate
  void ProcessNewConversionReports(
      std::vector<ConversionReport>* reports) override;
  int GetMaxConversionsPerImpression() const override;

  void OnInitCompleted(bool success);

  // Task runner used to perform operations on |storage_|. Runs with
  // base::TaskPriority::BEST_EFFORT.
  scoped_refptr<base::SequencedTaskRunner> storage_task_runner_;

  base::Clock* clock_;

  // ConversionStorage instance which is scoped to lifetime of
  // |storage_task_runner_|. |storage_| should be accessed by calling
  // base::PostTask with |storage_task_runner_|, and should not be accessed
  // directly.
  std::unique_ptr<ConversionStorage, base::OnTaskRunnerDeleter> storage_;

  // Policy used for controlling API configurations such as reporting and
  // attribution models. Unique ptr so it can be overridden for testing.
  std::unique_ptr<ConversionPolicy> conversion_policy_;

  base::WeakPtrFactory<ConversionManager> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_MANAGER_H_
