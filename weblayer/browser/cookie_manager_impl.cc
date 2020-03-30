// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/cookie_manager_impl.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/cookies/cookie_util.h"

#if defined(OS_ANDROID)
#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "weblayer/browser/java/jni/CookieManagerImpl_jni.h"
#endif

namespace weblayer {
namespace {

void GetCookieComplete(CookieManager::GetCookieCallback callback,
                       const net::CookieStatusList& cookies,
                       const net::CookieStatusList& excluded_cookies) {
  net::CookieList cookie_list = net::cookie_util::StripStatuses(cookies);
  std::move(callback).Run(net::CanonicalCookie::BuildCookieLine(cookie_list));
}

}  // namespace

CookieManagerImpl::CookieManagerImpl(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

void CookieManagerImpl::SetCookie(const GURL& url,
                                  const std::string& value,
                                  SetCookieCallback callback) {
  CHECK(SetCookieInternal(url, value, std::move(callback)));
}

void CookieManagerImpl::GetCookie(const GURL& url, GetCookieCallback callback) {
  content::BrowserContext::GetDefaultStoragePartition(browser_context_)
      ->GetCookieManagerForBrowserProcess()
      ->GetCookieList(url, net::CookieOptions::MakeAllInclusive(),
                      base::BindOnce(&GetCookieComplete, std::move(callback)));
}

#if defined(OS_ANDROID)
bool CookieManagerImpl::SetCookie(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& url,
    const base::android::JavaParamRef<jstring>& value,
    const base::android::JavaParamRef<jobject>& callback) {
  return SetCookieInternal(
      GURL(ConvertJavaStringToUTF8(url)), ConvertJavaStringToUTF8(value),
      base::BindOnce(&base::android::RunBooleanCallbackAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(callback)));
}

void CookieManagerImpl::GetCookie(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& url,
    const base::android::JavaParamRef<jobject>& callback) {
  GetCookie(
      GURL(ConvertJavaStringToUTF8(url)),
      base::BindOnce(&base::android::RunStringCallbackAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(callback)));
}
#endif

bool CookieManagerImpl::SetCookieInternal(const GURL& url,
                                          const std::string& value,
                                          SetCookieCallback callback) {
  auto cc = net::CanonicalCookie::Create(url, value, base::Time::Now(),
                                         base::nullopt);
  if (!cc) {
    return false;
  }

  content::BrowserContext::GetDefaultStoragePartition(browser_context_)
      ->GetCookieManagerForBrowserProcess()
      ->SetCanonicalCookie(*cc, url.scheme(),
                           net::CookieOptions::MakeAllInclusive(),
                           net::cookie_util::AdaptCookieInclusionStatusToBool(
                               std::move(callback)));
  return true;
}

}  // namespace weblayer
