// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_CONTEXT_MENU_HELPER_H_
#define CHROME_BROWSER_UI_ANDROID_CONTEXT_MENU_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/optimization_guide/proto/performance_hints_metadata.pb.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {
struct ContextMenuParams;
class RenderFrameHost;
class WebContents;
}

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.contextmenu
enum ContextMenuImageFormat {
  JPEG = 0,
  PNG = 1,
  ORIGINAL = 2,
};

class ContextMenuHelper
    : public content::WebContentsUserData<ContextMenuHelper> {
 protected:
  using ImageRetrieveCallback = base::Callback<void(
      mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
          chrome_render_frame_ptr,
      const base::android::JavaRef<jobject>& jcallback,
      const std::vector<uint8_t>& thumbnail_data,
      const gfx::Size& max_dimen_px,
      const std::string& image_extension)>;

 public:
  ~ContextMenuHelper() override;

  void ShowContextMenu(content::RenderFrameHost* render_frame_host,
                       const content::ContextMenuParams& params);

  void OnContextMenuClosed(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj);

  void SetPopulator(const base::android::JavaRef<jobject>& jpopulator);

  // Methods called from Java via JNI ------------------------------------------
  base::android::ScopedJavaLocalRef<jobject> GetJavaWebContents(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void OnStartDownload(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       jboolean jis_link);
  void SearchForImage(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj);
  void RetrieveImageForShare(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jcallback,
      jint max_width_px,
      jint max_height_px,
      jint j_image_type);
  void RetrieveImageForContextMenu(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jcallback,
      jint max_width_px,
      jint max_height_px);

 private:
  explicit ContextMenuHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<ContextMenuHelper>;

  void RetrieveImageInternal(
      JNIEnv* env,
      const ImageRetrieveCallback& retrieve_callback,
      const base::android::JavaParamRef<jobject>& jcallback,
      jint max_width_px,
      jint max_height_px,
      chrome::mojom::ImageFormat image_format);

  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
  GetChromeRenderFrame() const;

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
  content::WebContents* web_contents_;

  content::ContextMenuParams context_menu_params_;
  int render_frame_id_;
  int render_process_id_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(ContextMenuHelper);
};

#endif  // CHROME_BROWSER_UI_ANDROID_CONTEXT_MENU_HELPER_H_
