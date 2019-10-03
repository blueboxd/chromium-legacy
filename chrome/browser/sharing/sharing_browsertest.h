// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_BROWSERTEST_H_
#define CHROME_BROWSER_SHARING_SHARING_BROWSERTEST_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/gcm_driver/fake_gcm_profile_service.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "url/gurl.h"

// Base test class for testing sharing features.
class SharingBrowserTest : public SyncTest {
 public:
  SharingBrowserTest();

  ~SharingBrowserTest() override;

  void SetUpOnMainThread() override;

  void Init(const std::vector<base::Feature>& enabled_features,
            const std::vector<base::Feature>& disabled_features);

  virtual std::string GetTestPageURL() const = 0;

  void SetUpDevices(int count,
                    sync_pb::SharingSpecificFields_EnabledFeatures feature);

  std::unique_ptr<TestRenderViewContextMenu> InitRightClickMenu(
      const GURL& url,
      const base::string16& link_text,
      const base::string16& selection_text);

  void CheckLastReceiver(const std::string& device_guid) const;

  chrome_browser_sharing::SharingMessage GetLastSharingMessageSent() const;

  SharingService* sharing_service() const;

  content::WebContents* web_contents() const;

 private:
  gcm::GCMProfileServiceFactory::ScopedTestingFactoryInstaller
      scoped_testing_factory_installer_;
  base::test::ScopedFeatureList scoped_feature_list_;
  gcm::FakeGCMProfileService* gcm_service_;
  content::WebContents* web_contents_;
  syncer::FakeDeviceInfoTracker fake_device_info_tracker_;
  std::vector<std::unique_ptr<syncer::DeviceInfo>> device_infos_;
  SharingService* sharing_service_;

  DISALLOW_COPY_AND_ASSIGN(SharingBrowserTest);
};

#endif  // CHROME_BROWSER_SHARING_SHARING_BROWSERTEST_H_
