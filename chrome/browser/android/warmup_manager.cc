// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/WarmupManager_jni.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "content/public/browser/render_process_host.h"
#include "url/gurl.h"

using base::android::JavaParamRef;

static void JNI_WarmupManager_StartPreconnectPredictorInitialization(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  auto* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(profile);
  if (!loading_predictor)
    return;
  loading_predictor->StartInitialization();
}

static void JNI_WarmupManager_PreconnectUrlAndSubresources(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile,
    const JavaParamRef<jstring>& url_str) {
  if (url_str) {
    GURL url = GURL(base::android::ConvertJavaStringToUTF8(env, url_str));
    Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);

    auto* loading_predictor =
        predictors::LoadingPredictorFactory::GetForProfile(profile);
    if (loading_predictor) {
      loading_predictor->PrepareForPageLoad(url,
                                            predictors::HintOrigin::EXTERNAL);
    }
  }
}

static void JNI_WarmupManager_WarmupSpareRenderer(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  if (profile) {
    content::RenderProcessHost::WarmupSpareRenderProcessHost(profile);
  }
}

static void JNI_WarmupManager_ReportNextLikelyNavigations(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jprofile,
    const base::android::JavaParamRef<jobjectArray>& packages_str,
    const base::android::JavaParamRef<jobjectArray>& urls_str) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  NavigationPredictorKeyedService* predictor_service =
      NavigationPredictorKeyedServiceFactory::GetForProfile(profile);
  if (!predictor_service)
    return;

  jsize urls_length = env->GetArrayLength(urls_str);
  if (urls_length <= 0)
    return;

  std::vector<GURL> top_urls;
  top_urls.reserve(static_cast<size_t>(urls_length));
  for (size_t i = 0; i < static_cast<size_t>(urls_length); ++i) {
    base::android::ScopedJavaLocalRef<jstring> url_str(
        env, static_cast<jstring>(env->GetObjectArrayElement(urls_str, i)));

    GURL url(ConvertJavaStringToUTF8(url_str));
    if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS())
      continue;
    top_urls.push_back(url);
  }
  if (top_urls.empty())
    return;

  jsize packages_length = env->GetArrayLength(packages_str);
  if (packages_length <= 0)
    return;
  std::vector<std::string> external_packages;
  external_packages.reserve(static_cast<size_t>(packages_length));
  for (size_t i = 0; i < static_cast<size_t>(packages_length); ++i) {
    base::android::ScopedJavaLocalRef<jstring> package_str(
        env, static_cast<jstring>(env->GetObjectArrayElement(packages_str, i)));
    std::string package = ConvertJavaStringToUTF8(package_str);
    if (package.empty())
      continue;
    external_packages.push_back(package);
  }
  if (external_packages.empty())
    return;

  predictor_service->OnPredictionUpdatedByExternalAndroidApp(external_packages,
                                                             top_urls);
}
