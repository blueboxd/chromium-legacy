// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/ntp_snippets_bridge.h"

#include <jni.h>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/android/chrome_jni_headers/SnippetsBridge_jni.h"
#include "chrome/browser/android/ntp/get_remote_suggestions_scheduler.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/ntp_snippets/content_suggestions_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/history/core/browser/history_service.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/content_suggestions_metrics.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider.h"
#include "components/ntp_snippets/remote/remote_suggestions_scheduler.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaIntArrayToIntVector;
using base::android::AppendJavaStringArrayToStringVector;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaIntArray;
using ntp_snippets::Category;
using ntp_snippets::CategoryInfo;
using ntp_snippets::CategoryStatus;
using ntp_snippets::KnownCategories;
using ntp_snippets::ContentSuggestion;

static jlong JNI_SnippetsBridge_Init(JNIEnv* env,
                                     const JavaParamRef<jobject>& j_bridge,
                                     const JavaParamRef<jobject>& j_profile) {
  NTPSnippetsBridge* snippets_bridge =
      new NTPSnippetsBridge(env, j_bridge, j_profile);
  return reinterpret_cast<intptr_t>(snippets_bridge);
}

static void
JNI_SnippetsBridge_RemoteSuggestionsSchedulerOnPersistentSchedulerWakeUp(
    JNIEnv* env) {
  ntp_snippets::RemoteSuggestionsScheduler* scheduler =
      GetRemoteSuggestionsScheduler();
  if (!scheduler) {
    return;
  }

  scheduler->OnPersistentSchedulerWakeUp();
}

static void JNI_SnippetsBridge_RemoteSuggestionsSchedulerOnBrowserUpgraded(
    JNIEnv* env) {
  ntp_snippets::RemoteSuggestionsScheduler* scheduler =
      GetRemoteSuggestionsScheduler();
  // Can be null if the feature has been disabled but the scheduler has not been
  // unregistered yet. The next start should unregister it.
  if (!scheduler) {
    return;
  }

  scheduler->OnBrowserUpgraded();
}

NTPSnippetsBridge::NTPSnippetsBridge(JNIEnv* env,
                                     const JavaParamRef<jobject>& j_bridge,
                                     const JavaParamRef<jobject>& j_profile)
    : content_suggestions_service_observer_(this), bridge_(env, j_bridge) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  content_suggestions_service_ =
      ContentSuggestionsServiceFactory::GetForProfile(profile);
  history_service_ = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  content_suggestions_service_observer_.Add(content_suggestions_service_);

  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      feed::prefs::kArticlesListVisible,
      base::BindRepeating(
          &NTPSnippetsBridge::OnSuggestionsVisibilityChanged,
          base::Unretained(this),
          Category::FromKnownCategory(KnownCategories::ARTICLES)));
}

void NTPSnippetsBridge::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

ScopedJavaLocalRef<jintArray> NTPSnippetsBridge::GetCategories(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  std::vector<int> category_ids;
  for (Category category : content_suggestions_service_->GetCategories()) {
    category_ids.push_back(category.id());
  }
  return ToJavaIntArray(env, category_ids);
}

int NTPSnippetsBridge::GetCategoryStatus(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj,
                                         jint j_category_id) {
  return static_cast<int>(content_suggestions_service_->GetCategoryStatus(
      Category::FromIDValue(j_category_id)));
}

jboolean NTPSnippetsBridge::AreRemoteSuggestionsEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return content_suggestions_service_->AreRemoteSuggestionsEnabled();
}

void NTPSnippetsBridge::FetchSuggestionImage(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint j_category_id,
    const JavaParamRef<jstring>& id_within_category,
    const JavaParamRef<jobject>& j_callback) {
  ScopedJavaGlobalRef<jobject> callback(j_callback);
  content_suggestions_service_->FetchSuggestionImage(
      ContentSuggestion::ID(Category::FromIDValue(j_category_id),
                            ConvertJavaStringToUTF8(env, id_within_category)),
      base::BindOnce(&NTPSnippetsBridge::OnImageFetched,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void NTPSnippetsBridge::FetchSuggestionFavicon(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint j_category_id,
    const JavaParamRef<jstring>& id_within_category,
    jint j_minimum_size_px,
    jint j_desired_size_px,
    const JavaParamRef<jobject>& j_callback) {
  ScopedJavaGlobalRef<jobject> callback(j_callback);
  content_suggestions_service_->FetchSuggestionFavicon(
      ContentSuggestion::ID(Category::FromIDValue(j_category_id),
                            ConvertJavaStringToUTF8(env, id_within_category)),
      j_minimum_size_px, j_desired_size_px,
      base::BindOnce(&NTPSnippetsBridge::OnImageFetched,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void NTPSnippetsBridge::ReloadSuggestions(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  content_suggestions_service_->ReloadSuggestions();
}

void NTPSnippetsBridge::DismissSuggestion(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& jurl,
    jint global_position,
    jint j_category_id,
    jint position_in_category,
    const JavaParamRef<jstring>& id_within_category) {
  Category category = Category::FromIDValue(j_category_id);

  content_suggestions_service_->DismissSuggestion(ContentSuggestion::ID(
      category, ConvertJavaStringToUTF8(env, id_within_category)));

  history_service_->QueryURL(
      GURL(ConvertJavaStringToUTF8(env, jurl)), /*want_visits=*/false,
      base::BindOnce(
          [](int global_position, Category category, int position_in_category,
             history::QueryURLResult result) {
            bool visited = result.success && result.row.visit_count() != 0;
            ntp_snippets::metrics::OnSuggestionDismissed(
                global_position, category, position_in_category, visited);
          },
          global_position, category, position_in_category),
      &tracker_);
}

void NTPSnippetsBridge::DismissCategory(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        jint j_category_id) {
  Category category = Category::FromIDValue(j_category_id);

  content_suggestions_service_->DismissCategory(category);

  ntp_snippets::metrics::OnCategoryDismissed(category);
}

void NTPSnippetsBridge::RestoreDismissedCategories(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  content_suggestions_service_->RestoreDismissedCategories();
}

NTPSnippetsBridge::~NTPSnippetsBridge() {}

void NTPSnippetsBridge::OnNewSuggestions(Category category) {
  // TODO(https://crbug.com/1069183): Dead code, clean-up with rest of C++ Feed
  // code.
}

void NTPSnippetsBridge::OnCategoryStatusChanged(Category category,
                                                CategoryStatus new_status) {
  // TODO(https://crbug.com/1069183): Dead code, clean-up with rest of C++ Feed
  // code.
}

void NTPSnippetsBridge::OnSuggestionInvalidated(
    const ContentSuggestion::ID& suggestion_id) {
  // TODO(https://crbug.com/1069183): Dead code, clean-up with rest of C++ Feed
  // code.
}

void NTPSnippetsBridge::OnFullRefreshRequired() {
  // TODO(https://crbug.com/1069183): Dead code, clean-up with rest of C++ Feed
  // code.
}

void NTPSnippetsBridge::ContentSuggestionsServiceShutdown() {
  bridge_.Reset();
  content_suggestions_service_observer_.Remove(content_suggestions_service_);
}

void NTPSnippetsBridge::OnImageFetched(ScopedJavaGlobalRef<jobject> callback,
                                       const gfx::Image& image) {
  ScopedJavaLocalRef<jobject> j_bitmap;
  if (!image.IsEmpty()) {
    j_bitmap = gfx::ConvertToJavaBitmap(image.ToSkBitmap());
  }
  RunObjectCallbackAndroid(callback, j_bitmap);
}

void NTPSnippetsBridge::OnSuggestionsFetched(
    const ScopedJavaGlobalRef<jobject>& success_callback,
    const ScopedJavaGlobalRef<jobject>& failure_callback,
    Category category,
    ntp_snippets::Status status,
    std::vector<ContentSuggestion> suggestions) {
  // TODO(fhorschig, dgn): Allow refetch or show notification acc. to status.
  // Dead code, clean-up with rest of C++ Feed code.
}

void NTPSnippetsBridge::OnSuggestionsVisibilityChanged(
    const Category category) {
  // TODO(https://crbug.com/1069183): Dead code, clean-up with rest of C++ Feed
  // code.
}
