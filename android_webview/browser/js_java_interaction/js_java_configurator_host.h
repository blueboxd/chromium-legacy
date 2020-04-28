// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_JS_JAVA_CONFIGURATOR_HOST_H_
#define ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_JS_JAVA_CONFIGURATOR_HOST_H_

#include "android_webview/common/js_java_interaction/interfaces.mojom.h"
#include "base/android/scoped_java_ref.h"
#include "base/strings/string16.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace android_webview {

class AwOriginMatcher;
struct DocumentStartJavascript;
struct JsObject;

class JsToJavaMessaging;

// This class is 1:1 with WebContents, when AddWebMessageListener() is called,
// it stores the information in this class and send them to renderer side
// JsJavaConfigurator if there is any. When RenderFrameCreated() gets called, it
// needs to configure that new RenderFrame with the information stores in this
// class.
class JsJavaConfiguratorHost : public content::WebContentsObserver {
 public:
  explicit JsJavaConfiguratorHost(content::WebContents* web_contents);
  ~JsJavaConfiguratorHost() override;

  // Native side AddDocumentStartJavascript, returns an error message if the
  // parameters didn't pass necessary checks.
  jint AddDocumentStartJavascript(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& script,
      const base::android::JavaParamRef<jobjectArray>& allowed_origin_rules);

  jboolean RemoveDocumentStartJavascript(JNIEnv* env, jint script_id);

  // Native side AddWebMessageListener, returns an error message if the
  // parameters didn't pass necessary checks.
  base::android::ScopedJavaLocalRef<jstring> AddWebMessageListener(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& listener,
      const base::android::JavaParamRef<jstring>& js_object_name,
      const base::android::JavaParamRef<jobjectArray>& allowed_origin_rules);

  void RemoveWebMessageListener(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& js_object_name);

  base::android::ScopedJavaLocalRef<jobjectArray> GetJsObjectsInfo(
      JNIEnv* env,
      const base::android::JavaParamRef<jclass>& clazz);

  // content::WebContentsObserver implementations
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

 private:
  void NotifyFrameForWebMessageListener(
      content::RenderFrameHost* render_frame_host);
  void NotifyFrameForAllDocumentStartJavascripts(
      content::RenderFrameHost* render_frame_host);
  void NotifyFrameForAddDocumentStartJavascript(
      const DocumentStartJavascript* script,
      content::RenderFrameHost* render_frame_host);

  void NotifyFrameForRemoveDocumentStartJavascript(
      int32_t script_id,
      content::RenderFrameHost* render_frame_host);
  std::string ConvertToNativeAllowedOriginRulesWithSanityCheck(
      JNIEnv* env,
      const base::android::JavaParamRef<jobjectArray>& allowed_origin_rules,
      AwOriginMatcher& native_allowed_origin_rules);

  int32_t next_script_id_ = 0;
  std::vector<DocumentStartJavascript> scripts_;
  std::vector<JsObject> js_objects_;
  std::map<content::RenderFrameHost*,
           std::vector<std::unique_ptr<JsToJavaMessaging>>>
      js_to_java_messagings_;

  DISALLOW_COPY_AND_ASSIGN(JsJavaConfiguratorHost);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_JS_JAVA_CONFIGURATOR_HOST_H_
