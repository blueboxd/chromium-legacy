// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_test_signin_utils_android.h"

#include "base/android/jni_android.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/test/bind_test_util.h"
#include "chrome/test/sync_integration_test_support_jni_headers/SyncTestSigninUtils_jni.h"

namespace sync_test_signin_utils_android {

void SetUpTestAccountAndSignIn() {
  base::RunLoop run_loop;
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()}, base::BindLambdaForTesting([&]() {
        Java_SyncTestSigninUtils_setUpTestAccountAndSignIn(
            base::android::AttachCurrentThread());
        run_loop.Quit();
      }));
  run_loop.Run();
}

void SetUpAuthForTest() {
  Java_SyncTestSigninUtils_setUpAuthForTest(
      base::android::AttachCurrentThread());
}

void TearDownAuthForTest() {
  base::RunLoop run_loop;
  base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                             base::BindLambdaForTesting([&]() {
                               Java_SyncTestSigninUtils_tearDownAuthForTest(
                                   base::android::AttachCurrentThread());
                               run_loop.Quit();
                             }));
  run_loop.Run();
}

}  // namespace sync_test_signin_utils_android
