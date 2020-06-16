// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_CONFIRM_INFOBAR_ANDROID_H_
#define WEBLAYER_BROWSER_CONFIRM_INFOBAR_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "weblayer/browser/infobar_android.h"

namespace weblayer {

class ConfirmInfoBar : public InfoBarAndroid {
 public:
  explicit ConfirmInfoBar(std::unique_ptr<ConfirmInfoBarDelegate> delegate);
  ~ConfirmInfoBar() override;

 protected:
  ConfirmInfoBarDelegate* GetDelegate();
  base::string16 GetTextFor(ConfirmInfoBarDelegate::InfoBarButton button);

  // InfoBarAndroid overrides.
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env) override;

  void OnLinkClicked(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj) override;

  void ProcessButton(int action) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConfirmInfoBar);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_CONFIRM_INFOBAR_ANDROID_H_
