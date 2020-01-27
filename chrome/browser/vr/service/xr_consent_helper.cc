// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/service/xr_consent_helper.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#endif

namespace vr {

XrConsentHelper::XrConsentHelper() = default;
XrConsentHelper::~XrConsentHelper() = default;

// static
content::WebContents* XrConsentHelper::GetWebContentsFromRenderer(
    int render_process_id,
    int render_frame_id) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  DCHECK(render_frame_host);

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);

  return web_contents;
}

#if defined(OS_ANDROID)
// static
base::android::ScopedJavaLocalRef<jobject> XrConsentHelper::GetTabFromRenderer(
    int render_process_id,
    int render_frame_id) {
  auto* web_contents =
      GetWebContentsFromRenderer(render_process_id, render_frame_id);
  DCHECK(web_contents);

  TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents);
  DCHECK(tab_android);

  base::android::ScopedJavaLocalRef<jobject> j_tab_android =
      tab_android->GetJavaObject();
  DCHECK(!j_tab_android.is_null());

  return j_tab_android;
}
#endif
}  // namespace vr
