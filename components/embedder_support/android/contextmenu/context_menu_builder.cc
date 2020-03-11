// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/contextmenu/context_menu_builder.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/embedder_support/android/context_menu_jni_headers/ContextMenuParams_jni.h"
#include "content/public/browser/context_menu_params.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;

namespace context_menu {

base::android::ScopedJavaGlobalRef<jobject> BuildJavaContextMenuParams(
    const content::ContextMenuParams& params) {
  GURL sanitizedReferrer =
      (params.frame_url.is_empty() ? params.page_url : params.frame_url)
          .GetAsReferrer();

  bool can_save = params.media_flags & blink::WebContextMenuData::kMediaCanSave;
  JNIEnv* env = base::android::AttachCurrentThread();
  base::string16 title_text =
      (params.title_text.empty() ? params.alt_text : params.title_text);

  return base::android::ScopedJavaGlobalRef<jobject>(
      Java_ContextMenuParams_create(
          env, static_cast<int>(params.media_type),
          ConvertUTF8ToJavaString(env, params.page_url.spec()),
          ConvertUTF8ToJavaString(env, params.link_url.spec()),
          ConvertUTF16ToJavaString(env, params.link_text),
          ConvertUTF8ToJavaString(env, params.unfiltered_link_url.spec()),
          ConvertUTF8ToJavaString(env, params.src_url.spec()),
          ConvertUTF16ToJavaString(env, title_text),
          ConvertUTF8ToJavaString(env, sanitizedReferrer.spec()),
          static_cast<int>(params.referrer_policy), can_save, params.x,
          params.y, params.source_type));
}

}  // namespace context_menu
