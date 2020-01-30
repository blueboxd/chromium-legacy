// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>

#include "base/android/jni_android.h"
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/test/bind_test_util.h"
#include "components/metrics/metrics_switches.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "weblayer/browser/android/metrics/weblayer_metrics_service_client.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/public/navigation_controller.h"
#include "weblayer/public/profile.h"
#include "weblayer/public/tab.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browsertests_jni/MetricsTestHelper_jni.h"

namespace weblayer {

class MetricsBrowserTest : public WebLayerBrowserTest {
 public:
  void SetUp() override {
    instance_ = this;

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(metrics::switches::kForceEnableMetricsReporting);

    Java_MetricsTestHelper_installTestGmsBridge(
        base::android::AttachCurrentThread(), HasUserConsent());
    WebLayerMetricsServiceClient::GetInstance()->SetFastStartupForTesting(true);
    WebLayerMetricsServiceClient::GetInstance()->SetUploadIntervalForTesting(
        base::TimeDelta::FromMilliseconds(10));
    WebLayerBrowserTest::SetUp();
  }

  void TearDown() override {
    Java_MetricsTestHelper_removeTestGmsBridge(
        base::android::AttachCurrentThread());
    instance_ = nullptr;
    WebLayerBrowserTest::TearDown();
  }

  static void OnLogMetrics(const metrics::ChromeUserMetricsExtension& metric) {
    if (!instance_)
      return;
    instance_->metrics_logs_.push_back(metric);
    std::move(instance_->on_new_log_).Run();
  }

  metrics::ChromeUserMetricsExtension waitForNextMetricsLog() {
    if (metrics_logs_.empty()) {
      base::RunLoop run_loop;
      on_new_log_ = run_loop.QuitClosure();
      run_loop.Run();
      DCHECK(!metrics_logs_.empty());
    }
    metrics::ChromeUserMetricsExtension tmp = std::move(metrics_logs_.front());
    metrics_logs_.pop_front();
    return tmp;
  }

  size_t GetNumLogs() const { return metrics_logs_.size(); }

  virtual bool HasUserConsent() { return true; }

 private:
  std::unique_ptr<Profile> profile_;
  std::deque<metrics::ChromeUserMetricsExtension> metrics_logs_;
  base::OnceClosure on_new_log_;
  static MetricsBrowserTest* instance_;
};

MetricsBrowserTest* MetricsBrowserTest::instance_ = nullptr;

void JNI_MetricsTestHelper_OnLogMetrics(
    JNIEnv* env,
    const base::android::JavaParamRef<jbyteArray>& data) {
  metrics::ChromeUserMetricsExtension proto;
  jbyte* src_bytes = env->GetByteArrayElements(data, nullptr);
  proto.ParseFromArray(src_bytes, env->GetArrayLength(data.obj()));
  env->ReleaseByteArrayElements(data, src_bytes, JNI_ABORT);
  MetricsBrowserTest::OnLogMetrics(proto);
}

IN_PROC_BROWSER_TEST_F(MetricsBrowserTest, ProtoHasExpectedFields) {
  metrics::ChromeUserMetricsExtension log = waitForNextMetricsLog();
  EXPECT_EQ(metrics::ChromeUserMetricsExtension::ANDROID_WEBLAYER,
            log.product());
  EXPECT_TRUE(log.has_client_id());
  EXPECT_TRUE(log.has_session_id());

  const metrics::SystemProfileProto& system_profile = log.system_profile();
  EXPECT_TRUE(system_profile.has_build_timestamp());
  EXPECT_TRUE(system_profile.has_app_version());
  EXPECT_TRUE(system_profile.has_channel());
  EXPECT_TRUE(system_profile.has_install_date());
  EXPECT_TRUE(system_profile.has_application_locale());
  EXPECT_EQ("Android", system_profile.os().name());
  EXPECT_TRUE(system_profile.os().has_version());
  EXPECT_TRUE(system_profile.hardware().has_system_ram_mb());
  EXPECT_TRUE(system_profile.hardware().has_hardware_class());
  EXPECT_TRUE(system_profile.hardware().has_screen_count());
  EXPECT_TRUE(system_profile.hardware().has_primary_screen_width());
  EXPECT_TRUE(system_profile.hardware().has_primary_screen_height());
  EXPECT_TRUE(system_profile.hardware().has_primary_screen_scale_factor());
  EXPECT_TRUE(system_profile.hardware().has_cpu_architecture());
  EXPECT_TRUE(system_profile.hardware().cpu().has_vendor_name());
  EXPECT_TRUE(system_profile.hardware().cpu().has_signature());
  EXPECT_TRUE(system_profile.hardware().cpu().has_num_cores());
  EXPECT_TRUE(system_profile.hardware().cpu().has_is_hypervisor());
  EXPECT_TRUE(system_profile.hardware().gpu().has_driver_version());
  EXPECT_TRUE(system_profile.hardware().gpu().has_gl_vendor());
  EXPECT_TRUE(system_profile.hardware().gpu().has_gl_renderer());

  // We assert we do *not* log data which is deemed to be privacy sensitive.
  // It's OK to reverse these test conditions, but only if we log these fields
  // in a manner which is approved to preserve user privacy.
  EXPECT_FALSE(system_profile.hardware().has_bluetooth());
  EXPECT_FALSE(system_profile.hardware().has_usb());
}

IN_PROC_BROWSER_TEST_F(MetricsBrowserTest, PageLoadsEnableMultipleUploads) {
  waitForNextMetricsLog();

  // At this point, the MetricsService should be asleep, and should not have
  // created any more metrics logs.
  ASSERT_EQ(0u, GetNumLogs());

  // The start of a page load should be enough to indicate to the MetricsService
  // that the app is "in use" and it's OK to upload the next record. No need to
  // wait for onPageFinished, since this behavior should be gated on
  // NOTIFICATION_LOAD_START.
  shell()->tab()->GetNavigationController()->Navigate(GURL("about:blank"));

  // This may take slightly longer than UPLOAD_INTERVAL_MS, due to the time
  // spent processing the metrics log, but should be well within the timeout
  // (unless something is broken).
  waitForNextMetricsLog();

  // If we get here, we got a second metrics log (and the test may pass). If
  // there was no second metrics log, then the above call will check fail with a
  // timeout. We should not assert the logs are empty however, because it's
  // possible we got a metrics log between onPageStarted & onPageFinished, in
  // which case onPageFinished would *also* wake up the metrics service, and we
  // might potentially have a third metrics log in the queue.
}

class MetricsBrowserTestWithUserOptOut : public MetricsBrowserTest {
  bool HasUserConsent() override { return false; }
};

IN_PROC_BROWSER_TEST_F(MetricsBrowserTestWithUserOptOut, MetricsNotRecorded) {
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(0u, GetNumLogs());
}

}  // namespace weblayer
