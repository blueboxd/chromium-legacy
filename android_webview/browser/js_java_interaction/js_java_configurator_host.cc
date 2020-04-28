// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/js_java_interaction/js_java_configurator_host.h"

#include "android_webview/browser/js_java_interaction/js_to_java_messaging.h"
#include "android_webview/browser_jni_headers/WebMessageListenerInfo_jni.h"
#include "android_webview/common/aw_origin_matcher.h"
#include "android_webview/common/aw_origin_matcher_mojom_traits.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace android_webview {

struct JsObject {
  JsObject(base::string16 name,
           AwOriginMatcher allowed_origin_rules,
           const base::android::JavaRef<jobject>& listener)
      : name_(std::move(name)),
        allowed_origin_rules_(std::move(allowed_origin_rules)),
        listener_ref_(listener) {}

  JsObject(JsObject&& other)
      : name_(std::move(other.name_)),
        allowed_origin_rules_(std::move(other.allowed_origin_rules_)),
        listener_ref_(std::move(other.listener_ref_)) {}

  JsObject& operator=(JsObject&& other) {
    name_ = std::move(other.name_);
    allowed_origin_rules_ = std::move(other.allowed_origin_rules_);
    listener_ref_ = std::move(other.listener_ref_);
    return *this;
  }

  ~JsObject() = default;

  base::string16 name_;
  AwOriginMatcher allowed_origin_rules_;
  base::android::ScopedJavaGlobalRef<jobject> listener_ref_;

  DISALLOW_COPY_AND_ASSIGN(JsObject);
};

struct DocumentStartJavascript {
  DocumentStartJavascript(base::string16 script,
                          AwOriginMatcher allowed_origin_rules,
                          int32_t script_id)
      : script_(std::move(script)),
        allowed_origin_rules_(allowed_origin_rules),
        script_id_(script_id) {}

  DocumentStartJavascript(DocumentStartJavascript&) = delete;
  DocumentStartJavascript& operator=(DocumentStartJavascript&) = delete;
  DocumentStartJavascript(DocumentStartJavascript&&) = default;
  DocumentStartJavascript& operator=(DocumentStartJavascript&&) = default;

  base::string16 script_;
  AwOriginMatcher allowed_origin_rules_;
  int32_t script_id_;
};

JsJavaConfiguratorHost::JsJavaConfiguratorHost(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

JsJavaConfiguratorHost::~JsJavaConfiguratorHost() = default;

jint JsJavaConfiguratorHost::AddDocumentStartJavascript(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& script,
    const base::android::JavaParamRef<jobjectArray>& allowed_origin_rules) {
  base::string16 native_script =
      base::android::ConvertJavaStringToUTF16(env, script);
  AwOriginMatcher origin_matcher;
  std::string error_message = ConvertToNativeAllowedOriginRulesWithSanityCheck(
      env, allowed_origin_rules, origin_matcher);
  if (!error_message.empty()) {
    env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"),
                  error_message.data());
    // Invalid script id.
    return -1;
  }

  scripts_.emplace_back(native_script, origin_matcher, next_script_id_++);

  web_contents()->ForEachFrame(base::BindRepeating(
      &JsJavaConfiguratorHost::NotifyFrameForAddDocumentStartJavascript,
      base::Unretained(this), &*scripts_.rbegin()));
  return scripts_.rbegin()->script_id_;
}

jboolean JsJavaConfiguratorHost::RemoveDocumentStartJavascript(
    JNIEnv* env,
    jint j_script_id) {
  for (auto it = scripts_.begin(); it != scripts_.end(); ++it) {
    if (it->script_id_ == j_script_id) {
      scripts_.erase(it);
      web_contents()->ForEachFrame(base::BindRepeating(
          &JsJavaConfiguratorHost::NotifyFrameForRemoveDocumentStartJavascript,
          base::Unretained(this), j_script_id));
      return true;
    }
  }
  return false;
}

base::android::ScopedJavaLocalRef<jstring>
JsJavaConfiguratorHost::AddWebMessageListener(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& listener,
    const base::android::JavaParamRef<jstring>& js_object_name,
    const base::android::JavaParamRef<jobjectArray>& allowed_origin_rules) {
  base::string16 native_js_object_name =
      base::android::ConvertJavaStringToUTF16(env, js_object_name);

  AwOriginMatcher origin_matcher;
  std::string error_message = ConvertToNativeAllowedOriginRulesWithSanityCheck(
      env, allowed_origin_rules, origin_matcher);
  if (!error_message.empty())
    return base::android::ConvertUTF8ToJavaString(env, error_message);

  for (const auto& js_object : js_objects_) {
    if (js_object.name_ == native_js_object_name) {
      return base::android::ConvertUTF16ToJavaString(
          env, base::ASCIIToUTF16("jsObjectName ") + js_object.name_ +
                   base::ASCIIToUTF16(" is already added."));
    }
  }

  js_objects_.emplace_back(native_js_object_name, origin_matcher, listener);

  web_contents()->ForEachFrame(base::BindRepeating(
      &JsJavaConfiguratorHost::NotifyFrameForWebMessageListener,
      base::Unretained(this)));
  return nullptr;
}

void JsJavaConfiguratorHost::RemoveWebMessageListener(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& js_object_name) {
  base::string16 native_js_object_name =
      ConvertJavaStringToUTF16(env, js_object_name);

  for (auto iterator = js_objects_.begin(); iterator != js_objects_.end();
       ++iterator) {
    if (iterator->name_ == native_js_object_name) {
      js_objects_.erase(iterator);
      web_contents()->ForEachFrame(base::BindRepeating(
          &JsJavaConfiguratorHost::NotifyFrameForWebMessageListener,
          base::Unretained(this)));
      break;
    }
  }
}

base::android::ScopedJavaLocalRef<jobjectArray>
JsJavaConfiguratorHost::GetJsObjectsInfo(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& clazz) {
  jobjectArray joa =
      env->NewObjectArray(js_objects_.size(), clazz.obj(), nullptr);
  base::android::CheckException(env);

  for (size_t i = 0; i < js_objects_.size(); ++i) {
    const JsObject& js_object = js_objects_[i];
    std::vector<std::string> rules;
    for (const auto& rule : js_object.allowed_origin_rules_.rules())
      rules.push_back(rule->ToString());

    base::android::ScopedJavaLocalRef<jobject> object =
        Java_WebMessageListenerInfo_create(
            env, base::android::ConvertUTF16ToJavaString(env, js_object.name_),
            base::android::ToJavaArrayOfStrings(env, rules),
            js_object.listener_ref_);
    env->SetObjectArrayElement(joa, i, object.obj());
  }
  return base::android::ScopedJavaLocalRef<jobjectArray>(env, joa);
}

void JsJavaConfiguratorHost::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  NotifyFrameForWebMessageListener(render_frame_host);
  NotifyFrameForAllDocumentStartJavascripts(render_frame_host);
}

void JsJavaConfiguratorHost::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  js_to_java_messagings_.erase(render_frame_host);
}

void JsJavaConfiguratorHost::NotifyFrameForAllDocumentStartJavascripts(
    content::RenderFrameHost* render_frame_host) {
  for (const auto& script : scripts_) {
    NotifyFrameForAddDocumentStartJavascript(&script, render_frame_host);
  }
}

void JsJavaConfiguratorHost::NotifyFrameForWebMessageListener(
    content::RenderFrameHost* render_frame_host) {
  mojo::AssociatedRemote<mojom::JsJavaConfigurator> configurator_remote;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &configurator_remote);
  std::vector<mojom::JsObjectPtr> js_objects;
  js_objects.reserve(js_objects_.size());
  for (const auto& js_object : js_objects_) {
    mojo::PendingAssociatedRemote<mojom::JsToJavaMessaging> pending_remote;
    js_to_java_messagings_[render_frame_host].emplace_back(
        std::make_unique<JsToJavaMessaging>(
            render_frame_host,
            pending_remote.InitWithNewEndpointAndPassReceiver(),
            js_object.listener_ref_, js_object.allowed_origin_rules_));
    js_objects.push_back(mojom::JsObject::New(js_object.name_,
                                              std::move(pending_remote),
                                              js_object.allowed_origin_rules_));
  }
  configurator_remote->SetJsObjects(std::move(js_objects));
}

void JsJavaConfiguratorHost::NotifyFrameForAddDocumentStartJavascript(
    const DocumentStartJavascript* script,
    content::RenderFrameHost* render_frame_host) {
  DCHECK(script);
  mojo::AssociatedRemote<mojom::JsJavaConfigurator> configurator_remote;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &configurator_remote);
  configurator_remote->AddDocumentStartScript(
      mojom::DocumentStartJavascript::New(script->script_id_, script->script_,
                                          script->allowed_origin_rules_));
}

void JsJavaConfiguratorHost::NotifyFrameForRemoveDocumentStartJavascript(
    int32_t script_id,
    content::RenderFrameHost* render_frame_host) {
  mojo::AssociatedRemote<mojom::JsJavaConfigurator> configurator_remote;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &configurator_remote);
  configurator_remote->RemoveDocumentStartScript(script_id);
}

std::string
JsJavaConfiguratorHost::ConvertToNativeAllowedOriginRulesWithSanityCheck(
    JNIEnv* env,
    const base::android::JavaParamRef<jobjectArray>& allowed_origin_rules,
    AwOriginMatcher& native_allowed_origin_rules) {
  std::vector<std::string> native_allowed_origin_rule_strings;
  AppendJavaStringArrayToStringVector(env, allowed_origin_rules,
                                      &native_allowed_origin_rule_strings);
  for (auto& rule : native_allowed_origin_rule_strings) {
    if (!native_allowed_origin_rules.AddRuleFromString(rule)) {
      return "allowedOriginRules " + rule + " is invalid";
    }
  }
  return std::string();
}

}  // namespace android_webview
