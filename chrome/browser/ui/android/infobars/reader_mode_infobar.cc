// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/reader_mode_infobar.h"

#include <memory>
#include <utility>

#include "chrome/android/chrome_jni_headers/ReaderModeInfoBar_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

class ReaderModeInfoBarDelegate : public infobars::InfoBarDelegate {
 public:
  ~ReaderModeInfoBarDelegate() override {}

  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override {
    return InfoBarDelegate::InfoBarIdentifier::READER_MODE_INFOBAR_ANDROID;
  }

  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override {
    return delegate->GetIdentifier() == GetIdentifier();
  }
};

ReaderModeInfoBar::ReaderModeInfoBar(
    std::unique_ptr<ReaderModeInfoBarDelegate> delegate,
    const JavaParamRef<jobject>& j_manager)
    : InfoBarAndroid(std::move(delegate)), _j_reader_mode_manager(j_manager) {}

ReaderModeInfoBar::~ReaderModeInfoBar() {
  _j_reader_mode_manager.Reset();
}

infobars::InfoBarDelegate* ReaderModeInfoBar::GetDelegate() {
  return delegate();
}

ScopedJavaLocalRef<jobject> ReaderModeInfoBar::CreateRenderInfoBar(
    JNIEnv* env) {
  return Java_ReaderModeInfoBar_create(env);
}

base::android::ScopedJavaGlobalRef<jobject>
ReaderModeInfoBar::GetReaderModeManager(JNIEnv* env) {
  return _j_reader_mode_manager;
}

void ReaderModeInfoBar::ProcessButton(int action) {}

void JNI_ReaderModeInfoBar_Create(JNIEnv* env,
                                  const JavaParamRef<jobject>& j_tab,
                                  const JavaParamRef<jobject>& j_manager) {
  InfoBarService* service = InfoBarService::FromWebContents(
      TabAndroid::GetNativeTab(env, j_tab)->web_contents());

  service->AddInfoBar(std::make_unique<ReaderModeInfoBar>(
      std::make_unique<ReaderModeInfoBarDelegate>(), j_manager));
}
