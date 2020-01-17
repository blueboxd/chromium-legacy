// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/aw_attaching_to_window_recorder.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {
namespace {

struct {
  const base::TimeDelta interval;
  const char* histogram_name;
} kPingInterval[] = {
    {base::TimeDelta::FromSeconds(5), "Android.WebView.AttachedToWindowIn5s"},
    {base::TimeDelta::FromSeconds(30) - base::TimeDelta::FromSeconds(5),
     "Android.WebView.AttachedToWindowIn30s"},
    {base::TimeDelta::FromMinutes(3) - base::TimeDelta::FromSeconds(30),
     "Android.WebView.AttachedToWindowIn3m"}};

const char* kEverAttachedToWindow = "Android.WebView.EverAttachedToWindow";
const char* kAttachedToWindowTime = "Android.WebView.AttachedToWindowTime";

}  // namespace

class AwAttachingToWindowRecorderTest : public testing::Test {
 public:
  void SetUp() override {
    recorder_ = base::MakeRefCounted<AwAttachingToWindowRecorder>();
    recorder_->Start();
  }

  AwAttachingToWindowRecorder* recorder() { return recorder_.get(); }
  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  scoped_refptr<AwAttachingToWindowRecorder> recorder_;
};

TEST_F(AwAttachingToWindowRecorderTest, NeverAttached) {
  base::HistogramTester tester;
  task_environment().FastForwardBy(kPingInterval[0].interval);
  tester.ExpectBucketCount(kPingInterval[0].histogram_name, false, 1);

  task_environment().FastForwardBy(kPingInterval[1].interval);
  tester.ExpectBucketCount(kPingInterval[0].histogram_name, false, 1);
  tester.ExpectBucketCount(kPingInterval[1].histogram_name, false, 1);

  task_environment().FastForwardBy(kPingInterval[2].interval);
  tester.ExpectBucketCount(kPingInterval[0].histogram_name, false, 1);
  tester.ExpectBucketCount(kPingInterval[1].histogram_name, false, 1);
  tester.ExpectBucketCount(kPingInterval[2].histogram_name, false, 1);

  recorder()->OnDestroyed();
  task_environment().RunUntilIdle();
  tester.ExpectBucketCount(kPingInterval[0].histogram_name, false, 1);
  tester.ExpectBucketCount(kPingInterval[1].histogram_name, false, 1);
  tester.ExpectBucketCount(kPingInterval[2].histogram_name, false, 1);
  tester.ExpectBucketCount(kEverAttachedToWindow, false, 1);
}

TEST_F(AwAttachingToWindowRecorderTest, AttachedIn5s) {
  base::HistogramTester tester;
  recorder()->OnAttachedToWindow();
  task_environment().RunUntilIdle();
  tester.ExpectTotalCount(kAttachedToWindowTime, 1u);
  task_environment().FastForwardBy(kPingInterval[0].interval);
  tester.ExpectBucketCount(kPingInterval[0].histogram_name, true, 1);

  task_environment().FastForwardBy(kPingInterval[1].interval);
  tester.ExpectBucketCount(kPingInterval[0].histogram_name, true, 1);
  tester.ExpectBucketCount(kPingInterval[1].histogram_name, true, 1);

  task_environment().FastForwardBy(kPingInterval[2].interval);
  tester.ExpectBucketCount(kPingInterval[0].histogram_name, true, 1);
  tester.ExpectBucketCount(kPingInterval[1].histogram_name, true, 1);
  tester.ExpectBucketCount(kPingInterval[2].histogram_name, true, 1);

  recorder()->OnDestroyed();
  task_environment().RunUntilIdle();
  tester.ExpectBucketCount(kPingInterval[0].histogram_name, true, 1);
  tester.ExpectBucketCount(kPingInterval[1].histogram_name, true, 1);
  tester.ExpectBucketCount(kPingInterval[2].histogram_name, true, 1);
  tester.ExpectBucketCount(kEverAttachedToWindow, true, 1);
}

TEST_F(AwAttachingToWindowRecorderTest, AttachedIn30s) {
  base::HistogramTester tester;
  task_environment().FastForwardBy(kPingInterval[0].interval);
  tester.ExpectBucketCount(kPingInterval[0].histogram_name, false, 1);

  recorder()->OnAttachedToWindow();
  task_environment().RunUntilIdle();
  tester.ExpectTotalCount(kAttachedToWindowTime, 1u);
  task_environment().FastForwardBy(kPingInterval[1].interval);
  tester.ExpectBucketCount(kPingInterval[0].histogram_name, false, 1);
  tester.ExpectBucketCount(kPingInterval[1].histogram_name, true, 1);

  task_environment().FastForwardBy(kPingInterval[2].interval);
  tester.ExpectBucketCount(kPingInterval[0].histogram_name, false, 1);
  tester.ExpectBucketCount(kPingInterval[1].histogram_name, true, 1);
  tester.ExpectBucketCount(kPingInterval[2].histogram_name, true, 1);

  recorder()->OnDestroyed();
  task_environment().RunUntilIdle();
  tester.ExpectBucketCount(kPingInterval[0].histogram_name, false, 1);
  tester.ExpectBucketCount(kPingInterval[1].histogram_name, true, 1);
  tester.ExpectBucketCount(kPingInterval[2].histogram_name, true, 1);
  tester.ExpectBucketCount(kEverAttachedToWindow, true, 1);
}

TEST_F(AwAttachingToWindowRecorderTest, AttachedIn3m) {
  base::HistogramTester tester;
  task_environment().FastForwardBy(kPingInterval[0].interval);
  tester.ExpectBucketCount(kPingInterval[0].histogram_name, false, 1);

  task_environment().FastForwardBy(kPingInterval[1].interval);
  tester.ExpectBucketCount(kPingInterval[0].histogram_name, false, 1);
  tester.ExpectBucketCount(kPingInterval[1].histogram_name, false, 1);

  recorder()->OnAttachedToWindow();
  task_environment().RunUntilIdle();
  tester.ExpectTotalCount(kAttachedToWindowTime, 1u);
  task_environment().FastForwardBy(kPingInterval[2].interval);
  tester.ExpectBucketCount(kPingInterval[0].histogram_name, false, 1);
  tester.ExpectBucketCount(kPingInterval[1].histogram_name, false, 1);
  tester.ExpectBucketCount(kPingInterval[2].histogram_name, true, 1);

  recorder()->OnDestroyed();
  task_environment().RunUntilIdle();
  tester.ExpectBucketCount(kPingInterval[0].histogram_name, false, 1);
  tester.ExpectBucketCount(kPingInterval[1].histogram_name, false, 1);
  tester.ExpectBucketCount(kPingInterval[2].histogram_name, true, 1);
  tester.ExpectBucketCount(kEverAttachedToWindow, true, 1);
}

}  // namespace android_webview
