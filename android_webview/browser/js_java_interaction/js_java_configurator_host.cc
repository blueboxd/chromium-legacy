// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/js_java_interaction/js_java_configurator_host.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace url {
class Origin;
}

namespace android_webview {

JsJavaConfiguratorHost::JsJavaConfiguratorHost(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

JsJavaConfiguratorHost::~JsJavaConfiguratorHost() = default;

base::android::ScopedJavaLocalRef<jstring>
JsJavaConfiguratorHost::SetJsApiService(
    JNIEnv* env,
    bool need_to_inject_js_object,
    const base::android::JavaParamRef<jstring>& js_object_name,
    const base::android::JavaParamRef<jobjectArray>& allowed_origin_rules) {
  std::string native_js_object_name;
  base::android::ConvertJavaStringToUTF8(env, js_object_name,
                                         &native_js_object_name);

  std::vector<std::string> native_allowed_origin_rules;
  AppendJavaStringArrayToStringVector(env, allowed_origin_rules,
                                      &native_allowed_origin_rules);

  need_to_inject_js_object_ = need_to_inject_js_object;
  js_object_name_ = native_js_object_name;
  allowed_origin_rules_ = net::ProxyBypassRules();
  for (auto& rule : native_allowed_origin_rules) {
    if (!allowed_origin_rules_.AddRuleFromString(rule)) {
      return base::android::ConvertUTF8ToJavaString(
          env, "allowedOriginRules " + rule + " is invalid");
    }
  }

  web_contents()->ForEachFrame(base::BindRepeating(
      &JsJavaConfiguratorHost::NotifyFrame, base::Unretained(this)));
  return nullptr;
}

bool JsJavaConfiguratorHost::IsOriginAllowedForOnPostMessage(
    const url::Origin& origin) {
  return allowed_origin_rules_.Matches(origin.GetURL());
}

void JsJavaConfiguratorHost::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  NotifyFrame(render_frame_host);
}

void JsJavaConfiguratorHost::NotifyFrame(
    content::RenderFrameHost* render_frame_host) {
  mojo::AssociatedRemote<mojom::JsJavaConfigurator> configurator_remote;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &configurator_remote);
  configurator_remote->SetJsApiService(need_to_inject_js_object_,
                                       js_object_name_, allowed_origin_rules_);
}

}  // namespace android_webview
