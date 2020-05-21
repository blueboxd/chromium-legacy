// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting_service_settings.h"
#include "base/json/json_reader.h"
#include "base/no_destructor.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

struct TestParam {
  TestParam(const char* settings_value, ReportingSettings* expected_settings)
      : settings_value(settings_value), expected_settings(expected_settings) {}

  const char* settings_value;
  ReportingSettings* expected_settings;
};

constexpr char kNormalSettings[] = R"({ "service_provider": "Google" })";

constexpr char kNoProviderSettings[] = "{}";

ReportingSettings* NormalSettings() {
  static base::NoDestructor<ReportingSettings> settings;
  return settings.get();
}

ReportingSettings* NoSettings() {
  return nullptr;
}

}  // namespace

class ReportingServiceSettingsTest : public testing::TestWithParam<TestParam> {
 public:
  const char* settings_value() const { return GetParam().settings_value; }
  ReportingSettings* expected_settings() const {
    return GetParam().expected_settings;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_P(ReportingServiceSettingsTest, Test) {
  auto settings = base::JSONReader::Read(settings_value(),
                                         base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(settings.has_value());

  ReportingServiceSettings service_settings(settings.value());

  auto reporting_settings = service_settings.GetReportingSettings();
  ASSERT_EQ((expected_settings() != nullptr), reporting_settings.has_value());
}

INSTANTIATE_TEST_CASE_P(
    ,
    ReportingServiceSettingsTest,
    testing::Values(TestParam(kNormalSettings, NormalSettings()),
                    TestParam(kNoProviderSettings, NoSettings())));

}  // namespace enterprise_connectors
