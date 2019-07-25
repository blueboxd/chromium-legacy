// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/hints_component_util.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/version.h"
#include "components/optimization_guide/hints_component_info.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

const base::FilePath::CharType kFileName[] = FILE_PATH_LITERAL("somefile.pb");

class HintsComponentUtilTest : public testing::Test {
 public:
  HintsComponentUtilTest() {}

  ~HintsComponentUtilTest() override {}

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void WriteConfigToFile(const base::FilePath& filePath,
                         const proto::Configuration& config) {
    std::string serialized_config;
    ASSERT_TRUE(config.SerializeToString(&serialized_config));
    ASSERT_EQ(static_cast<int32_t>(serialized_config.length()),
              base::WriteFile(filePath, serialized_config.data(),
                              serialized_config.length()));
  }

  base::FilePath temp_dir() const { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(HintsComponentUtilTest);
};

TEST_F(HintsComponentUtilTest, RecordProcessHintsComponentResult) {
  base::HistogramTester histogram_tester;
  RecordProcessHintsComponentResult(ProcessHintsComponentResult::kSuccess);
  histogram_tester.ExpectUniqueSample("OptimizationGuide.ProcessHintsResult",
                                      ProcessHintsComponentResult::kSuccess, 1);
}

TEST_F(HintsComponentUtilTest, RecordOptimizationFilterStatus) {
  base::HistogramTester histogram_tester;
  RecordOptimizationFilterStatus(
      proto::OptimizationType::NOSCRIPT,
      OptimizationFilterStatus::kFoundServerBlacklistConfig);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.OptimizationFilterStatus.NoScript",
      OptimizationFilterStatus::kFoundServerBlacklistConfig, 1);
}

TEST_F(HintsComponentUtilTest, ProcessHintsComponentInvalidVersion) {
  ProcessHintsComponentResult result;
  std::unique_ptr<proto::Configuration> config = ProcessHintsComponent(
      HintsComponentInfo(base::Version(""), base::FilePath(kFileName)),
      &result);

  EXPECT_FALSE(config);
  EXPECT_EQ(ProcessHintsComponentResult::kFailedInvalidParameters, result);
}

TEST_F(HintsComponentUtilTest, ProcessHintsComponentInvalidPath) {
  ProcessHintsComponentResult result;
  std::unique_ptr<proto::Configuration> config = ProcessHintsComponent(
      HintsComponentInfo(base::Version("1.0.0.0"), base::FilePath()), &result);

  EXPECT_FALSE(config);
  EXPECT_EQ(ProcessHintsComponentResult::kFailedInvalidParameters, result);
}

TEST_F(HintsComponentUtilTest, ProcessHintsComponentInvalidFile) {
  ProcessHintsComponentResult result;
  std::unique_ptr<proto::Configuration> config = ProcessHintsComponent(
      HintsComponentInfo(base::Version("1.0.0"), base::FilePath(kFileName)),
      &result);

  EXPECT_FALSE(config);
  EXPECT_EQ(ProcessHintsComponentResult::kFailedReadingFile, result);
}

TEST_F(HintsComponentUtilTest, ProcessHintsComponentNotAConfigInFile) {
  const base::FilePath filePath = temp_dir().Append(kFileName);
  ASSERT_EQ(static_cast<int32_t>(3), base::WriteFile(filePath, "boo", 3));

  ProcessHintsComponentResult result;
  std::unique_ptr<proto::Configuration> config = ProcessHintsComponent(
      HintsComponentInfo(base::Version("1.0.0"), filePath), &result);

  EXPECT_FALSE(config);
  EXPECT_EQ(ProcessHintsComponentResult::kFailedInvalidConfiguration, result);
}

TEST_F(HintsComponentUtilTest, ProcessHintsComponentSuccess) {
  const base::FilePath filePath = temp_dir().Append(kFileName);
  proto::Configuration config;
  proto::Hint* hint = config.add_hints();
  hint->set_key("google.com");
  ASSERT_NO_FATAL_FAILURE(WriteConfigToFile(filePath, config));

  ProcessHintsComponentResult result;
  std::unique_ptr<proto::Configuration> processed_config =
      ProcessHintsComponent(
          HintsComponentInfo(base::Version("1.0.0"), filePath), &result);

  ASSERT_TRUE(processed_config);
  EXPECT_EQ(1, processed_config->hints_size());
  EXPECT_EQ("google.com", processed_config->hints()[0].key());
  EXPECT_EQ(ProcessHintsComponentResult::kSuccess, result);
}

TEST_F(HintsComponentUtilTest,
       ProcessHintsComponentSuccessNoResultProvidedDoesntCrash) {
  const base::FilePath filePath = temp_dir().Append(kFileName);
  proto::Configuration config;
  proto::Hint* hint = config.add_hints();
  hint->set_key("google.com");
  ASSERT_NO_FATAL_FAILURE(WriteConfigToFile(filePath, config));

  std::unique_ptr<proto::Configuration> processed_config =
      ProcessHintsComponent(
          HintsComponentInfo(base::Version("1.0.0"), filePath), nullptr);

  ASSERT_TRUE(processed_config);
  EXPECT_EQ(1, processed_config->hints_size());
  EXPECT_EQ("google.com", processed_config->hints()[0].key());
}

}  // namespace optimization_guide
