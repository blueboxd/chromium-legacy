// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/services/storage/public/mojom/storage_service.mojom.h"
#include "components/services/storage/public/mojom/test_api.test-mojom.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"

namespace content {
namespace {

// TODO(https://crbug.com/1052045): Enable this on Android once we have
// sandboxing for Storage Service. We do not support unsandboxed service
// processes on Android.
#if !defined(OS_ANDROID)

class StorageServiceRestartBrowserTest : public ContentBrowserTest {
 public:
  StorageServiceRestartBrowserTest() {
    // These tests only make sense when the service is running out-of-process.
    feature_list_.InitAndEnableFeature(features::kStorageServiceOutOfProcess);
  }

  mojo::Remote<storage::mojom::TestApi>& GetTestApi() {
    if (!test_api_) {
      StoragePartitionImpl::GetStorageServiceForTesting()->BindTestApi(
          test_api_.BindNewPipeAndPassReceiver().PassPipe());
    }
    return test_api_;
  }

  void CrashStorageServiceAndWaitForRestart() {
    mojo::Remote<storage::mojom::StorageService>& service =
        StoragePartitionImpl::GetStorageServiceForTesting();
    base::RunLoop loop;
    service.set_disconnect_handler(base::BindLambdaForTesting([&] {
      loop.Quit();
      service.reset();
    }));
    GetTestApi()->CrashNow();
    loop.Run();
    test_api_.reset();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  mojo::Remote<storage::mojom::TestApi> test_api_;
};

IN_PROC_BROWSER_TEST_F(StorageServiceRestartBrowserTest, BasicReconnect) {
  // Basic smoke test to ensure that we can force-crash the service and
  // StoragePartitionImpl will internally re-establish a working connection to
  // a new process.
  GetTestApi().FlushForTesting();
  EXPECT_TRUE(GetTestApi().is_connected());
  CrashStorageServiceAndWaitForRestart();
  GetTestApi().FlushForTesting();
  EXPECT_TRUE(GetTestApi().is_connected());
}
#endif  // !defined(OS_ANDROID)

}  // namespace
}  // namespace content
