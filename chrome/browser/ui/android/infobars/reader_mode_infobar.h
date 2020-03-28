// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_READER_MODE_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_READER_MODE_INFOBAR_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/ui/android/infobars/infobar_android.h"
#include "components/infobars/core/infobar_delegate.h"

class ReaderModeInfoBarDelegate;

class ReaderModeInfoBar : public InfoBarAndroid {
 public:
  explicit ReaderModeInfoBar(
      std::unique_ptr<ReaderModeInfoBarDelegate> delegate,
      const base::android::JavaParamRef<jobject>& j_manager);
  ~ReaderModeInfoBar() override;

  base::android::ScopedJavaGlobalRef<jobject> GetReaderModeManager(JNIEnv* env);

 protected:
  infobars::InfoBarDelegate* GetDelegate();

  // InfoBarAndroid overrides.
  void ProcessButton(int action) override;
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> _j_reader_mode_manager;
  DISALLOW_COPY_AND_ASSIGN(ReaderModeInfoBar);
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_READER_MODE_INFOBAR_H_
