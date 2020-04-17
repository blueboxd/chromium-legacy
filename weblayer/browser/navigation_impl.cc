// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/navigation_impl.h"

#include "content/public/browser/navigation_handle.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"

#if defined(OS_ANDROID)
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "weblayer/browser/java/jni/NavigationImpl_jni.h"
#endif

#if defined(OS_ANDROID)
using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;
#endif

namespace weblayer {

NavigationImpl::NavigationImpl(content::NavigationHandle* navigation_handle)
    : navigation_handle_(navigation_handle) {}

NavigationImpl::~NavigationImpl() {
#if defined(OS_ANDROID)
  if (java_navigation_) {
    Java_NavigationImpl_onNativeDestroyed(AttachCurrentThread(),
                                          java_navigation_);
  }
#endif
}

void NavigationImpl::SetParamsToLoadWhenSafe(
    std::unique_ptr<content::NavigationController::LoadURLParams> params) {
  scheduled_load_params_ = std::move(params);
}

std::unique_ptr<content::NavigationController::LoadURLParams>
NavigationImpl::TakeParamsToLoadWhenSafe() {
  return std::move(scheduled_load_params_);
}

#if defined(OS_ANDROID)
void NavigationImpl::SetJavaNavigation(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_navigation) {
  java_navigation_ = java_navigation;
}

ScopedJavaLocalRef<jstring> NavigationImpl::GetUri(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return ScopedJavaLocalRef<jstring>(
      base::android::ConvertUTF8ToJavaString(env, GetURL().spec()));
}

ScopedJavaLocalRef<jobjectArray> NavigationImpl::GetRedirectChain(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  std::vector<std::string> jni_redirects;
  for (const GURL& redirect : GetRedirectChain())
    jni_redirects.push_back(redirect.spec());
  return base::android::ToJavaArrayOfStrings(env, jni_redirects);
}

jboolean NavigationImpl::SetRequestHeader(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& name,
    const base::android::JavaParamRef<jstring>& value) {
  if (!safe_to_set_request_headers_)
    return false;

  SetRequestHeader(ConvertJavaStringToUTF8(name),
                   ConvertJavaStringToUTF8(value));
  return true;
}

#endif

GURL NavigationImpl::GetURL() {
  return navigation_handle_->GetURL();
}

const std::vector<GURL>& NavigationImpl::GetRedirectChain() {
  return navigation_handle_->GetRedirectChain();
}

NavigationState NavigationImpl::GetState() {
  if (navigation_handle_->IsErrorPage())
    return NavigationState::kFailed;
  if (navigation_handle_->HasCommitted())
    return NavigationState::kComplete;
  if (navigation_handle_->GetResponseHeaders())
    return NavigationState::kReceivingBytes;
  return NavigationState::kWaitingResponse;
}

int NavigationImpl::GetHttpStatusCode() {
  auto* response_headers = navigation_handle_->GetResponseHeaders();
  return response_headers ? response_headers->response_code() : 0;
}

bool NavigationImpl::IsSameDocument() {
  return navigation_handle_->IsSameDocument();
}

bool NavigationImpl::IsErrorPage() {
  return navigation_handle_->IsErrorPage();
}

Navigation::LoadError NavigationImpl::GetLoadError() {
  auto error_code = navigation_handle_->GetNetErrorCode();
  if (auto* response_headers = navigation_handle_->GetResponseHeaders()) {
    auto response_code = response_headers->response_code();
    if (response_code >= 400 && response_code < 500)
      return kHttpClientError;
    if (response_code >= 500 && response_code < 600)
      return kHttpServerError;
  }

  if (error_code == net::OK)
    return kNoError;

  if (net::IsCertificateError(error_code))
    return kSSLError;

  if (error_code <= -100 && error_code > -200)
    return kConnectivityError;

  return kOtherError;
}

void NavigationImpl::SetRequestHeader(const std::string& name,
                                      const std::string& value) {
  navigation_handle_->SetRequestHeader(name, value);
}

#if defined(OS_ANDROID)
static jboolean JNI_NavigationImpl_IsValidRequestHeaderName(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& name) {
  return net::HttpUtil::IsValidHeaderName(ConvertJavaStringToUTF8(name));
}

static jboolean JNI_NavigationImpl_IsValidRequestHeaderValue(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& value) {
  return net::HttpUtil::IsValidHeaderValue(ConvertJavaStringToUTF8(value));
}
#endif

}  // namespace weblayer
