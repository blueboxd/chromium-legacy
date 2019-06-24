// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/TranslateBridge_jni.h"
#include "chrome/browser/language/language_model_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/common/language_experiments.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "content/public/browser/web_contents.h"

static ChromeTranslateClient* GetTranslateClient(
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  ChromeTranslateClient* client =
      ChromeTranslateClient::FromWebContents(web_contents);
  DCHECK(client);
  return client;
}

static void JNI_TranslateBridge_ManualTranslateWhenReady(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  ChromeTranslateClient* client =
      ChromeTranslateClient::FromWebContents(web_contents);
  DCHECK(client);
  client->ManualTranslateWhenReady();
}

static jboolean JNI_TranslateBridge_CanManuallyTranslate(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  ChromeTranslateClient* client = GetTranslateClient(j_web_contents);
  translate::TranslateManager* manager = client->GetTranslateManager();
  DCHECK(manager);
  return manager->CanManuallyTranslate();
}

static jboolean JNI_TranslateBridge_ShouldShowManualTranslateIPH(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  ChromeTranslateClient* client = GetTranslateClient(j_web_contents);
  translate::TranslateManager* manager = client->GetTranslateManager();
  DCHECK(manager);

  const std::string page_lang = manager->GetLanguageState().original_language();
  std::unique_ptr<translate::TranslatePrefs> translate_prefs(
      client->GetTranslatePrefs());

  return base::StartsWith(page_lang, "en",
                          base::CompareCase::INSENSITIVE_ASCII) &&
         !language::ShouldForceTriggerTranslateOnEnglishPages(
             translate_prefs->GetForceTriggerOnEnglishPagesCount()) &&
         !manager->GetLanguageState().translate_enabled();
}

static void JNI_TranslateBridge_SetPredefinedTargetLanguage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    const base::android::JavaParamRef<jstring>& j_translate_language) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  const std::string translate_language(
      ConvertJavaStringToUTF8(env, j_translate_language));

  ChromeTranslateClient* client =
      ChromeTranslateClient::FromWebContents(web_contents);
  DCHECK(client);
  client->SetPredefinedTargetLanguage(translate_language);
}

// Returns the preferred target language to translate into for this user.
static base::android::ScopedJavaLocalRef<jstring>
JNI_TranslateBridge_GetTargetLanguage(JNIEnv* env) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  language::LanguageModel* language_model =
      LanguageModelManagerFactory::GetForBrowserContext(profile)
          ->GetPrimaryModel();
  DCHECK(language_model);
  PrefService* pref_service = profile->GetPrefs();
  std::string target_language =
      TranslateService::GetTargetLanguage(pref_service, language_model);
  DCHECK(!target_language.empty());
  base::android::ScopedJavaLocalRef<jstring> j_target_language =
      base::android::ConvertUTF8ToJavaString(env, target_language);
  return j_target_language;
}

// Determines whether the given language is blocked for translation.
static jboolean JNI_TranslateBridge_IsBlockedLanguage(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& j_language_string) {
  std::string language_we_might_block =
      ConvertJavaStringToUTF8(env, j_language_string);
  Profile* profile = ProfileManager::GetActiveUserProfile();
  PrefService* pref_service = profile->GetPrefs();
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(pref_service);
  DCHECK(translate_prefs);
  return translate_prefs->IsBlockedLanguage(language_we_might_block);
}

// Gets all the model languages and calls back to the Java
// TranslateBridge#addModelLanguageToSet once for each language.
static void JNI_TranslateBridge_GetModelLanguages(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& set) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  language::LanguageModel* language_model =
      LanguageModelManagerFactory::GetForBrowserContext(profile)
          ->GetPrimaryModel();
  DCHECK(language_model);
  std::string model_languages;
  std::vector<language::LanguageModel::LanguageDetails> languageDetails =
      language_model->GetLanguages();
  DCHECK(!languageDetails.empty());
  for (const auto& details : languageDetails) {
    Java_TranslateBridge_addModelLanguageToSet(
        env, set,
        base::android::ConvertUTF8ToJavaString(env, details.lang_code));
  }
}
