// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/sms/sms_infobar.h"

#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/SmsReceiverInfoBar_jni.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/android/sms/sms_infobar_delegate.h"
#include "url/origin.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ScopedJavaLocalRef;
using infobars::InfoBarDelegate;

// static
void SmsInfoBar::Create(InfoBarService* infobar_service,
                        const url::Origin& origin,
                        base::OnceClosure on_confirm,
                        base::OnceClosure on_cancel) {
  auto delegate = std::make_unique<SmsInfoBarDelegate>(
      origin, std::move(on_confirm), std::move(on_cancel));
  auto infobar = std::make_unique<SmsInfoBar>(std::move(delegate));
  infobar_service->AddInfoBar(std::move(infobar));
}

SmsInfoBar::SmsInfoBar(std::unique_ptr<SmsInfoBarDelegate> delegate)
    : ConfirmInfoBar(std::move(delegate)) {}

SmsInfoBar::~SmsInfoBar() = default;

ScopedJavaLocalRef<jobject> SmsInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  SmsInfoBarDelegate* delegate =
      static_cast<SmsInfoBarDelegate*>(GetDelegate());

  auto title = ConvertUTF16ToJavaString(env, delegate->GetTitle());
  auto message = ConvertUTF16ToJavaString(env, delegate->GetMessageText());
  auto button = ConvertUTF16ToJavaString(
      env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_OK));

  return Java_SmsReceiverInfoBar_create(env, GetEnumeratedIconId(), title,
                                        message, button);
}
