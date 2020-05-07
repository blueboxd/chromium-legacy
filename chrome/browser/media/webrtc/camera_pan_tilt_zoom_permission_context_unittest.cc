// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/camera_pan_tilt_zoom_permission_context.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace {

struct ContentSettingTestParams {
  const ContentSetting initial_camera_pan_tilt_zoom;
  const ContentSetting mediastream_camera;
  const ContentSetting expected_camera_pan_tilt_zoom;
} kTestParams[] = {
    {CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK,
     CONTENT_SETTING_ASK},  // Granted permission is reset if camera is
                            // blocked.
    {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK,
     CONTENT_SETTING_ASK},  // Granted permission is reset if camera is
                            // reset.
    {CONTENT_SETTING_BLOCK, CONTENT_SETTING_ALLOW,
     CONTENT_SETTING_BLOCK},  // Blocked permission is not reset if camera
                              // is granted.
    {CONTENT_SETTING_BLOCK, CONTENT_SETTING_BLOCK,
     CONTENT_SETTING_BLOCK},  // Blocked permission is not reset if camera
                              // is blocked.
};

}  // namespace

// Waits until a camera change is observed in content settings.
class CameraContentSettingsChangeWaiter : public content_settings::Observer {
 public:
  explicit CameraContentSettingsChangeWaiter(Profile* profile)
      : profile_(profile) {
    HostContentSettingsMapFactory::GetForProfile(profile)->AddObserver(this);
  }
  ~CameraContentSettingsChangeWaiter() override {
    HostContentSettingsMapFactory::GetForProfile(profile_)->RemoveObserver(
        this);
  }

  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const std::string& resource_identifier) override {
    if (content_type == ContentSettingsType::MEDIASTREAM_CAMERA)
      Proceed();
  }

  void Wait() { run_loop_.Run(); }

 private:
  void Proceed() { run_loop_.Quit(); }

  Profile* profile_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(CameraContentSettingsChangeWaiter);
};

class CameraPanTiltZoomPermissionContextTests
    : public ChromeRenderViewHostTestHarness,
      public testing::WithParamInterface<ContentSettingTestParams> {
 public:
  CameraPanTiltZoomPermissionContextTests() = default;

  void SetContentSetting(ContentSettingsType content_settings_type,
                         ContentSetting content_setting) {
    GURL url("https://www.example.com");
    HostContentSettingsMap* content_settings =
        HostContentSettingsMapFactory::GetForProfile(profile());
    content_settings->SetContentSettingDefaultScope(
        url, GURL(), content_settings_type, std::string(), content_setting);
  }

  ContentSetting GetContentSetting(ContentSettingsType content_settings_type) {
    GURL url("https://www.example.com");
    HostContentSettingsMap* content_settings =
        HostContentSettingsMapFactory::GetForProfile(profile());
    return content_settings->GetContentSetting(
        url.GetOrigin(), url.GetOrigin(), content_settings_type, std::string());
  }

  DISALLOW_COPY_AND_ASSIGN(CameraPanTiltZoomPermissionContextTests);
};

TEST_P(CameraPanTiltZoomPermissionContextTests,
       TestResetPermissionOnCameraChange) {
  CameraPanTiltZoomPermissionContext permission_context(profile());
  CameraContentSettingsChangeWaiter waiter(profile());

  SetContentSetting(ContentSettingsType::CAMERA_PAN_TILT_ZOOM,
                    GetParam().initial_camera_pan_tilt_zoom);
  SetContentSetting(ContentSettingsType::MEDIASTREAM_CAMERA,
                    GetParam().mediastream_camera);

  waiter.Wait();
  EXPECT_EQ(GetParam().expected_camera_pan_tilt_zoom,
            GetContentSetting(ContentSettingsType::CAMERA_PAN_TILT_ZOOM));
}

INSTANTIATE_TEST_SUITE_P(ResetPermissionOnCameraChange,
                         CameraPanTiltZoomPermissionContextTests,
                         testing::ValuesIn(kTestParams));
