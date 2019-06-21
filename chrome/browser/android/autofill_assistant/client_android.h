// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_CLIENT_ANDROID_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_CLIENT_ANDROID_H_

#include <map>
#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/autofill_assistant/ui_controller_android.h"
#include "components/autofill_assistant/browser/access_token_fetcher.h"
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/controller.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill_assistant {

// Creates a Autofill Assistant client associated with a WebContents.
//
// To obtain an instance of this class from the C++ side, call
// ClientAndroid::FromWebContents(web_contents). To make sure an instance
// exists, call ClientAndroid::CreateForWebContents first.
//
// From the Java side, call AutofillAssistantClient.fromWebContents.
//
// This class is accessible from the Java side through AutofillAssistantClient.
class ClientAndroid : public Client,
                      public AccessTokenFetcher,
                      public content::WebContentsUserData<ClientAndroid> {
 public:
  ~ClientAndroid() override;

  // Returns the corresponding Java AutofillAssistantClient.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  void Start(JNIEnv* env,
             const base::android::JavaParamRef<jobject>& jcaller,
             const base::android::JavaParamRef<jstring>& jinitial_url,
             const base::android::JavaParamRef<jstring>& jexperiment_ids,
             const base::android::JavaParamRef<jobjectArray>& parameterNames,
             const base::android::JavaParamRef<jobjectArray>& parameterValues,
             const base::android::JavaParamRef<jobject>& joverlay_coordinator);
  void DestroyUI(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& jcaller);
  void TransferUITo(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jobject>& jother_web_contents);

  base::android::ScopedJavaLocalRef<jstring> GetPrimaryAccountName(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);
  void OnAccessToken(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& jcaller,
                     jboolean success,
                     const base::android::JavaParamRef<jstring>& access_token);

  // Overrides Client
  void ShowUI() override;
  void DestroyUI() override;
  std::string GetApiKey() override;
  std::string GetAccountEmailAddress() override;
  AccessTokenFetcher* GetAccessTokenFetcher() override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  std::string GetServerUrl() override;
  UiController* GetUiController() override;
  std::string GetLocale() override;
  std::string GetCountryCode() override;
  void Shutdown(Metrics::DropOutReason reason) override;

  // Overrides AccessTokenFetcher
  void FetchAccessToken(
      base::OnceCallback<void(bool, const std::string&)>) override;
  void InvalidateAccessToken(const std::string& access_token) override;

 private:
  friend class content::WebContentsUserData<ClientAndroid>;

  explicit ClientAndroid(content::WebContents* web_contents);
  void CreateController();
  void DestroyController();
  void CreateUI(
      const base::android::JavaParamRef<jobject>& joverlay_coordinator);
  bool NeedsUI();
  void SetUI(std::unique_ptr<UiControllerAndroid> ui_controller_android);

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  content::WebContents* web_contents_;

  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  std::unique_ptr<Controller> controller_;
  std::unique_ptr<UiControllerAndroid> ui_controller_android_;
  base::OnceCallback<void(bool, const std::string&)>
      fetch_access_token_callback_;
  std::string server_url_;

  base::WeakPtrFactory<ClientAndroid> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ClientAndroid);
};

}  // namespace autofill_assistant.
#endif  // CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_CLIENT_ANDROID_H_
