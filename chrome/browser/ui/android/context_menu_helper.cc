// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/context_menu_helper.h"

#include <stdint.h>

#include <map>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/strings/string_util.h"
#include "chrome/android/chrome_jni_headers/ContextMenuHelper_jni.h"
#include "chrome/browser/download/android/download_controller_base.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "chrome/browser/performance_hints/performance_hints_observer.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/embedder_support/android/contextmenu/context_menu_builder.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/context_menu_data/media_type.h"
#include "ui/android/view_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using optimization_guide::proto::PerformanceClass;

namespace {

chrome::mojom::ImageFormat ToChromeMojomImageFormat(int image_format) {
  ContextMenuImageFormat format =
      static_cast<ContextMenuImageFormat>(image_format);
  switch (format) {
    case ContextMenuImageFormat::JPEG:
      return chrome::mojom::ImageFormat::JPEG;
    case ContextMenuImageFormat::PNG:
      return chrome::mojom::ImageFormat::PNG;
    case ContextMenuImageFormat::ORIGINAL:
      return chrome::mojom::ImageFormat::ORIGINAL;
  }

  NOTREACHED();
  return chrome::mojom::ImageFormat::JPEG;
}

class ContextMenuHelperImageRequest : public ImageDecoder::ImageRequest {
 public:
  static void Start(const base::android::JavaRef<jobject>& jcallback,
                    const std::vector<uint8_t>& thumbnail_data) {
    ContextMenuHelperImageRequest* request =
        new ContextMenuHelperImageRequest(jcallback);
    ImageDecoder::Start(request, thumbnail_data);
  }

 protected:
  void OnImageDecoded(const SkBitmap& decoded_image) override {
    base::android::RunObjectCallbackAndroid(
        jcallback_, gfx::ConvertToJavaBitmap(&decoded_image));
    delete this;
  }

  void OnDecodeImageFailed() override {
    base::android::ScopedJavaLocalRef<jobject> j_bitmap;
    base::android::RunObjectCallbackAndroid(jcallback_, j_bitmap);
    delete this;
  }

 private:
  ContextMenuHelperImageRequest(
      const base::android::JavaRef<jobject>& jcallback)
      : jcallback_(jcallback) {}

  const base::android::ScopedJavaGlobalRef<jobject> jcallback_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ContextMenuHelperImageRequest);
};

void OnRetrieveImageForShare(
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
        chrome_render_frame,
    const base::android::JavaRef<jobject>& jcallback,
    const std::vector<uint8_t>& thumbnail_data,
    const gfx::Size& original_size,
    const std::string& image_extension) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jbyteArray> j_data =
      base::android::ToJavaByteArray(env, thumbnail_data);
  base::android::ScopedJavaLocalRef<jstring> j_extension =
      base::android::ConvertUTF8ToJavaString(env, image_extension);
  base::android::RunObjectCallbackAndroid(
      jcallback, Java_ContextMenuHelper_createImageCallbackResult(env, j_data,
                                                                  j_extension));
}

void OnRetrieveImageForContextMenu(
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
        chrome_render_frame,
    const base::android::JavaRef<jobject>& jcallback,
    const std::vector<uint8_t>& thumbnail_data,
    const gfx::Size& original_size,
    const std::string& filename_extension) {
  ContextMenuHelperImageRequest::Start(jcallback, thumbnail_data);
}

}  // namespace

ContextMenuHelper::ContextMenuHelper(content::WebContents* web_contents)
    : web_contents_(web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(
      env, Java_ContextMenuHelper_create(env, reinterpret_cast<int64_t>(this),
                                         web_contents_->GetJavaWebContents())
               .obj());
  DCHECK(!java_obj_.is_null());
}

ContextMenuHelper::~ContextMenuHelper() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextMenuHelper_destroy(env, java_obj_);
}

void ContextMenuHelper::ShowContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  // TODO(crbug.com/851495): support context menu in VR.
  if (vr::VrTabHelper::IsUiSuppressedInVr(
          web_contents_, vr::UiSuppressedElement::kContextMenu)) {
    web_contents_->NotifyContextMenuClosed(params.custom_context);
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  context_menu_params_ = params;
  render_frame_id_ = render_frame_host->GetRoutingID();
  render_process_id_ = render_frame_host->GetProcess()->GetID();
  gfx::NativeView view = web_contents_->GetNativeView();
  if (!params.link_url.is_empty()) {
    PerformanceHintsObserver::RecordPerformanceUMAForURL(web_contents_,
                                                         params.link_url);
  }
  Java_ContextMenuHelper_showContextMenu(
      env, java_obj_, context_menu::BuildJavaContextMenuParams(params),
      view->GetContainerView(), view->content_offset() * view->GetDipScale());
}

void ContextMenuHelper::OnContextMenuClosed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  web_contents_->NotifyContextMenuClosed(context_menu_params_.custom_context);
}

void ContextMenuHelper::SetPopulator(const JavaRef<jobject>& jpopulator) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextMenuHelper_setPopulator(env, java_obj_, jpopulator);
}

base::android::ScopedJavaLocalRef<jobject>
ContextMenuHelper::GetJavaWebContents(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  return web_contents_->GetJavaWebContents();
}

void ContextMenuHelper::OnStartDownload(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        jboolean jis_link) {
  std::string headers;
  DownloadControllerBase::Get()->StartContextMenuDownload(
      context_menu_params_,
      web_contents_,
      jis_link,
      headers);
}

void ContextMenuHelper::SearchForImage(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  if (!render_frame_host)
    return;

  CoreTabHelper::FromWebContents(web_contents_)->SearchByImageInNewTab(
      render_frame_host, context_menu_params_.src_url);
}

void ContextMenuHelper::RetrieveImageForShare(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jcallback,
    jint max_width_px,
    jint max_height_px,
    jint j_image_format) {
  RetrieveImageInternal(env, base::Bind(&OnRetrieveImageForShare), jcallback,
                        max_width_px, max_height_px,
                        ToChromeMojomImageFormat(j_image_format));
}

void ContextMenuHelper::RetrieveImageForContextMenu(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jcallback,
    jint max_width_px,
    jint max_height_px) {
  // For context menu, Image needs to be PNG for receiving transparency pixels.
  RetrieveImageInternal(env, base::Bind(&OnRetrieveImageForContextMenu),
                        jcallback, max_width_px, max_height_px,
                        chrome::mojom::ImageFormat::PNG);
}

void ContextMenuHelper::RetrieveImageInternal(
    JNIEnv* env,
    const ImageRetrieveCallback& retrieve_callback,
    const JavaParamRef<jobject>& jcallback,
    jint max_width_px,
    jint max_height_px,
    chrome::mojom::ImageFormat image_format) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  if (!render_frame_host)
    return;
  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &chrome_render_frame);

  // Bind the InterfacePtr into the callback so that it's kept alive
  // until there's either a connection error or a response.
  auto* thumbnail_capturer_proxy = chrome_render_frame.get();
  thumbnail_capturer_proxy->RequestImageForContextNode(
      max_width_px * max_height_px, gfx::Size(max_width_px, max_height_px),
      image_format,
      base::BindOnce(
          retrieve_callback, base::Passed(&chrome_render_frame),
          base::android::ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContextMenuHelper)
