// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CONTEXT_MENU_CHROME_CONTEXT_MENU_POPULATOR_H_
#define CHROME_BROWSER_ANDROID_CONTEXT_MENU_CHROME_CONTEXT_MENU_POPULATOR_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {
class WebContents;
}

class ImageRetrieveCallback;

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.contextmenu
enum ContextMenuImageFormat {
  JPEG = 0,
  PNG = 1,
  ORIGINAL = 2,
};

// Performs context menu-related actions.
class ChromeContextMenuPopulator {
 protected:
  using ImageRetrieveCallback = base::Callback<void(
      mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
          chrome_render_frame_ptr,
      const base::android::JavaRef<jobject>& jcallback,
      const std::vector<uint8_t>& thumbnail_data,
      const gfx::Size& max_dimen_px,
      const std::string& image_extension)>;

 public:
  explicit ChromeContextMenuPopulator(content::WebContents* const web_contents);

  void OnStartDownload(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jcontext_menu_params,
      jboolean jis_link);
  void SearchForImage(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jrender_frame_host,
      const base::android::JavaParamRef<jobject>& jcontext_menu_params);
  void RetrieveImageForShare(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jrender_frame_host,
      const base::android::JavaParamRef<jobject>& jcallback,
      jint max_width_px,
      jint max_height_px,
      jint j_image_type);
  void RetrieveImageForContextMenu(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jrender_frame_host,
      const base::android::JavaParamRef<jobject>& jcallback,
      jint max_width_px,
      jint max_height_px);

 private:
  void RetrieveImageInternal(
      JNIEnv* env,
      const ImageRetrieveCallback& retrieve_callback,
      const base::android::JavaParamRef<jobject>& jrender_frame_host,
      const base::android::JavaParamRef<jobject>& jcallback,
      jint max_width_px,
      jint max_height_px,
      chrome::mojom::ImageFormat image_format);

  content::WebContents* const web_contents_;
};

#endif  // CHROME_BROWSER_ANDROID_CONTEXT_MENU_CHROME_CONTEXT_MENU_POPULATOR_H_
