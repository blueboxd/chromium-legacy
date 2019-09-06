// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "content/public/app/content_jni_onload.h"
#include "content/public/app/content_main.h"
#include "weblayer/app/content_main_delegate_impl.h"

namespace weblayer {
class MainDelegateImpl : public MainDelegate {
 public:
  void PreMainMessageLoopRun() override {}
  void SetMainMessageLoopQuitClosure(base::OnceClosure quit_closure) override {}
};
}  // namespace weblayer

// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  if (!content::android::OnJNIOnLoadInit())
    return -1;
  weblayer::MainParams params;
  params.delegate = new weblayer::MainDelegateImpl;
  params.pak_name = "weblayer_support.pak";
  params.brand = "WebLayer";

  content::SetContentMainDelegate(
      new weblayer::ContentMainDelegateImpl(params));
  return JNI_VERSION_1_4;
}
