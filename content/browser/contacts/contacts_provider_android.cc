// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/contacts/contacts_provider_android.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_string.h"
#include "base/callback.h"
#include "components/url_formatter/elide_url.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/android/content_jni_headers/ContactsDialogHost_jni.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "ui/android/window_android.h"
#include "url/origin.h"

namespace content {

ContactsProviderAndroid::ContactsProviderAndroid(
    RenderFrameHostImpl* render_frame_host) {
  JNIEnv* env = base::android::AttachCurrentThread();

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return;
  if (!web_contents->GetTopLevelNativeWindow())
    return;

  formatted_origin_ = url_formatter::FormatUrlForSecurityDisplay(
      render_frame_host->GetLastCommittedOrigin().GetURL(),
      url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);

  dialog_.Reset(Java_ContactsDialogHost_create(
      env, web_contents->GetTopLevelNativeWindow()->GetJavaObject(),
      reinterpret_cast<intptr_t>(this)));
  DCHECK(!dialog_.is_null());
}

ContactsProviderAndroid::~ContactsProviderAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContactsDialogHost_destroy(env, dialog_);
}

void ContactsProviderAndroid::Select(
    bool multiple,
    bool include_names,
    bool include_emails,
    bool include_tel,
    blink::mojom::ContactsManager::SelectCallback callback) {
  if (!dialog_) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  callback_ = std::move(callback);

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContactsDialogHost_showDialog(
      env, dialog_, multiple, include_names, include_emails, include_tel,
      base::android::ConvertUTF16ToJavaString(env, formatted_origin_));
}

void ContactsProviderAndroid::AddContact(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jboolean include_names,
    jboolean include_emails,
    jboolean include_tel,
    const base::android::JavaParamRef<jobjectArray>& names_java,
    const base::android::JavaParamRef<jobjectArray>& emails_java,
    const base::android::JavaParamRef<jobjectArray>& tel_java) {
  DCHECK(callback_);

  base::Optional<std::vector<std::string>> names;
  if (include_names) {
    std::vector<std::string> names_vector;
    AppendJavaStringArrayToStringVector(env, names_java, &names_vector);
    names = names_vector;
  }

  base::Optional<std::vector<std::string>> emails;
  if (include_emails) {
    std::vector<std::string> emails_vector;
    AppendJavaStringArrayToStringVector(env, emails_java, &emails_vector);
    emails = emails_vector;
  }

  base::Optional<std::vector<std::string>> tel;
  if (include_tel) {
    std::vector<std::string> tel_vector;
    AppendJavaStringArrayToStringVector(env, tel_java, &tel_vector);
    tel = tel_vector;
  }

  blink::mojom::ContactInfoPtr contact =
      blink::mojom::ContactInfo::New(names, emails, tel);

  contacts_.push_back(std::move(contact));
}

void ContactsProviderAndroid::EndContactsList(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DCHECK(callback_);
  std::move(callback_).Run(std::move(contacts_));
}

void ContactsProviderAndroid::EndWithPermissionDenied(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DCHECK(callback_);
  std::move(callback_).Run(base::nullopt);
}

}  // namespace content
