// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_TEST_UTILS_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_TEST_UTILS_H_

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "content/browser/conversions/conversion_manager.h"
#include "content/browser/conversions/conversion_manager_impl.h"
#include "content/browser/conversions/conversion_report.h"
#include "content/browser/conversions/conversion_storage.h"
#include "content/browser/conversions/storable_conversion.h"
#include "content/browser/conversions/storable_impression.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/origin.h"

namespace content {

class PassThroughStorageDelegate : public ConversionStorage::Delegate {
 public:
  PassThroughStorageDelegate() = default;
  ~PassThroughStorageDelegate() override = default;

  // ConversionStorage::Delegate
  void ProcessNewConversionReports(
      std::vector<ConversionReport>* reports) override {}

  int GetMaxConversionsPerImpression() const override;
};

// Test manager provider which can be used to inject a fake ConversionManager.
class TestManagerProvider : public ConversionManager::Provider {
 public:
  explicit TestManagerProvider(ConversionManager* manager)
      : manager_(manager) {}
  ~TestManagerProvider() override = default;

  ConversionManager* GetManager(WebContents* web_contents) const override;

 private:
  ConversionManager* manager_ = nullptr;
};

// Test ConversionManager which can be injected into tests to monitor calls to a
// ConversionManager instance.
class TestConversionManager : public ConversionManager {
 public:
  TestConversionManager();
  ~TestConversionManager() override;

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

  void SetActiveImpressionsForWebUI(
      std::vector<StorableImpression> impressions);
  void SetReportsForWebUI(std::vector<ConversionReport> reports);

  // Resets all counters on this.
  void Reset();

  size_t num_impressions() const { return num_impressions_; }
  size_t num_conversions() const { return num_conversions_; }

  int64_t last_sent_report_id() { return last_sent_report_id_; }

 private:
  ConversionPolicy policy_;
  size_t num_impressions_ = 0;
  size_t num_conversions_ = 0;
  int64_t last_sent_report_id_ = 0L;

  std::vector<StorableImpression> impressions_;
  std::vector<ConversionReport> reports_;
};

// Helper class to construct a StorableImpression for tests using default data.
// StorableImpression members are not mutable after construction requiring a
// builder pattern.
class ImpressionBuilder {
 public:
  explicit ImpressionBuilder(base::Time time);
  ~ImpressionBuilder();

  ImpressionBuilder& SetExpiry(base::TimeDelta delta);

  ImpressionBuilder& SetData(const std::string& data);

  ImpressionBuilder& SetImpressionOrigin(const url::Origin& origin);

  ImpressionBuilder& SetConversionOrigin(const url::Origin& origin);

  ImpressionBuilder& SetReportingOrigin(const url::Origin& origin);

  StorableImpression Build() const;

 private:
  std::string impression_data_;
  base::Time impression_time_;
  base::TimeDelta expiry_;
  url::Origin impression_origin_;
  url::Origin conversion_origin_;
  url::Origin reporting_origin_;
};

// Returns a StorableConversion with default data which matches the default
// impressions created by ImpressionBuilder.
StorableConversion DefaultConversion();

testing::AssertionResult ImpressionsEqual(const StorableImpression& expected,
                                          const StorableImpression& actual);

testing::AssertionResult ReportsEqual(
    const std::vector<ConversionReport>& expected,
    const std::vector<ConversionReport>& actual);

std::vector<ConversionReport> GetConversionsToReportForTesting(
    ConversionManagerImpl* manager,
    base::Time max_report_time);

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_TEST_UTILS_H_
