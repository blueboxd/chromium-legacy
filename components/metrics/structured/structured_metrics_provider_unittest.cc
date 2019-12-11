// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/structured_metrics_provider.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/metrics/structured/event_base.h"
#include "components/metrics/structured/recorder.h"
#include "components/prefs/json_pref_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace structured {

class StructuredMetricsProviderTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    storage_ = new JsonPrefStore(temp_dir_.GetPath().Append("storage.json"));
    provider_ = std::make_unique<StructuredMetricsProvider>();
  }

  base::FilePath TempDirPath() { return temp_dir_.GetPath(); }

  void Wait() { task_environment_.RunUntilIdle(); }

  void BeginInit() { return provider_->OnProfileAdded(TempDirPath()); }

  bool is_initialized() { return provider_->initialized_; }
  bool is_recording_enabled() { return provider_->recording_enabled_; }

 private:
  std::unique_ptr<StructuredMetricsProvider> provider_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  base::ScopedTempDir temp_dir_;
  scoped_refptr<JsonPrefStore> storage_;
};

TEST_F(StructuredMetricsProviderTest, ProviderInitializesFromBlankSlate) {
  BeginInit();
  Wait();
  EXPECT_TRUE(is_initialized());
}

// TODO(crbug.com/1016655): Add more tests once codegen for EventBase subclasses
// has been merged.

}  // namespace structured
}  // namespace metrics
