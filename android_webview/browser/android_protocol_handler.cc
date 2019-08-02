// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/android_protocol_handler.h"

#include <memory>
#include <utility>

#include "android_webview/browser/input_stream.h"
#include "android_webview/common/url_constants.h"
#include "android_webview/native_jni/AndroidProtocolHandler_jni.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "content/public/common/url_constants.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_interceptor.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using android_webview::InputStream;
using android_webview::InputStream;
using base::android::AttachCurrentThread;
using base::android::ClearException;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace {

const void* const kPreviouslyFailedKey = &kPreviouslyFailedKey;

class AndroidRequestInterceptorBase : public net::URLRequestInterceptor {
 public:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override;

  virtual bool ShouldHandleRequest(const net::URLRequest* request) const = 0;
};

class AssetFileRequestInterceptor : public AndroidRequestInterceptorBase {
 public:
  AssetFileRequestInterceptor();
  bool ShouldHandleRequest(const net::URLRequest* request) const override;
};

// Protocol handler for content:// scheme requests.
class ContentSchemeRequestInterceptor : public AndroidRequestInterceptorBase {
 public:
  ContentSchemeRequestInterceptor();
  bool ShouldHandleRequest(const net::URLRequest* request) const override;
};

// AndroidRequestInterceptorBase ----------------------------------------------

net::URLRequestJob* AndroidRequestInterceptorBase::MaybeInterceptRequest(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  NOTREACHED();
  return nullptr;
}

// AssetFileRequestInterceptor ------------------------------------------------

AssetFileRequestInterceptor::AssetFileRequestInterceptor() {}

bool AssetFileRequestInterceptor::ShouldHandleRequest(
    const net::URLRequest* request) const {
  return android_webview::IsAndroidSpecialFileUrl(request->url());
}

// ContentSchemeRequestInterceptor --------------------------------------------

ContentSchemeRequestInterceptor::ContentSchemeRequestInterceptor() {}

bool ContentSchemeRequestInterceptor::ShouldHandleRequest(
    const net::URLRequest* request) const {
  return request->url().SchemeIs(url::kContentScheme);
}

}  // namespace

namespace android_webview {

// static
std::unique_ptr<net::URLRequestInterceptor>
CreateContentSchemeRequestInterceptor() {
  return std::make_unique<ContentSchemeRequestInterceptor>();
}

// static
std::unique_ptr<net::URLRequestInterceptor>
CreateAssetFileRequestInterceptor() {
  return std::unique_ptr<net::URLRequestInterceptor>(
      new AssetFileRequestInterceptor());
}

// static
std::unique_ptr<InputStream> CreateInputStream(JNIEnv* env, const GURL& url) {
  DCHECK(url.is_valid());
  DCHECK(env);

  // Open the input stream.
  ScopedJavaLocalRef<jstring> jurl = ConvertUTF8ToJavaString(env, url.spec());
  ScopedJavaLocalRef<jobject> stream =
      android_webview::Java_AndroidProtocolHandler_open(env, jurl);

  if (stream.is_null()) {
    DLOG(ERROR) << "Unable to open input stream for Android URL";
    return nullptr;
  }
  return std::make_unique<InputStream>(stream);
}

bool GetInputStreamMimeType(JNIEnv* env,
                            const GURL& url,
                            InputStream* stream,
                            std::string* mime_type) {
  // Query the mime type from the Java side. It is possible for the query to
  // fail, as the mime type cannot be determined for all supported schemes.
  ScopedJavaLocalRef<jstring> java_url =
      ConvertUTF8ToJavaString(env, url.spec());
  ScopedJavaLocalRef<jstring> returned_type =
      android_webview::Java_AndroidProtocolHandler_getMimeType(
          env, stream->jobj(), java_url);
  if (returned_type.is_null())
    return false;

  *mime_type = base::android::ConvertJavaStringToUTF8(returned_type);
  return true;
}

static ScopedJavaLocalRef<jstring>
JNI_AndroidProtocolHandler_GetAndroidAssetPath(JNIEnv* env) {
  return ConvertUTF8ToJavaString(env, android_webview::kAndroidAssetPath);
}

static ScopedJavaLocalRef<jstring>
JNI_AndroidProtocolHandler_GetAndroidResourcePath(JNIEnv* env) {
  return ConvertUTF8ToJavaString(env, android_webview::kAndroidResourcePath);
}

}  // namespace android_webview
