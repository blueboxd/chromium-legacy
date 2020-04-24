// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/android/payment_app_service_bridge.h"

#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/memory/singleton.h"
#include "base/notreached.h"
#include "chrome/browser/payments/android/jni_headers/PaymentAppServiceBridge_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_data_service_factory.h"
#include "components/payments/content/android/byte_buffer_helper.h"
#include "components/payments/content/payment_app_service.h"
#include "components/payments/content/payment_app_service_factory.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/android/gurl_android.h"
#include "url/origin.h"

namespace {
using ::base::android::AttachCurrentThread;
using ::base::android::ConvertJavaStringToUTF8;
using ::base::android::ConvertUTF8ToJavaString;
using ::base::android::JavaParamRef;
using ::base::android::JavaRef;
using ::base::android::ScopedJavaGlobalRef;
using ::base::android::ScopedJavaLocalRef;
using ::base::android::ToJavaArrayOfStrings;
using ::base::android::ToJavaIntArray;
using ::payments::android::DeserializeFromJavaByteBufferArray;
using ::payments::mojom::PaymentMethodDataPtr;

// Helper to get the PaymentAppService associated with |render_frame_host|'s
// WebContents.
payments::PaymentAppService* GetPaymentAppService(
    content::RenderFrameHost* render_frame_host) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  return payments::PaymentAppServiceFactory::GetForContext(
      web_contents ? web_contents->GetBrowserContext() : nullptr);
}

void OnPaymentAppsCreated(
    const JavaRef<jobject>& jcallback,
    const content::PaymentAppProvider::PaymentApps& apps,
    const payments::ServiceWorkerPaymentAppFinder::InstallablePaymentApps&
        installable_apps) {
  JNIEnv* env = AttachCurrentThread();

  for (const auto& app_info : apps) {
    // Sends related application Ids to java side if the app prefers related
    // applications.
    std::vector<std::string> preferred_related_application_ids;
    if (app_info.second->prefer_related_applications) {
      for (const auto& related_application :
           app_info.second->related_applications) {
        // Only consider related applications on Google play for Android.
        if (related_application.platform == "play")
          preferred_related_application_ids.emplace_back(
              related_application.id);
      }
    }

    base::android::ScopedJavaLocalRef<jobjectArray> jcapabilities =
        Java_PaymentAppServiceBridge_createCapabilities(
            env, app_info.second->capabilities.size());
    for (size_t i = 0; i < app_info.second->capabilities.size(); i++) {
      Java_PaymentAppServiceBridge_addCapabilities(
          env, jcapabilities, base::checked_cast<int>(i),
          ToJavaIntArray(
              env, app_info.second->capabilities[i].supported_card_networks));
    }

    base::android::ScopedJavaLocalRef<jobject> jsupported_delegations =
        Java_PaymentAppServiceBridge_createSupportedDelegations(
            env, app_info.second->supported_delegations.shipping_address,
            app_info.second->supported_delegations.payer_name,
            app_info.second->supported_delegations.payer_phone,
            app_info.second->supported_delegations.payer_email);

    // TODO(crbug.com/846077): Find a proper way to make use of user hint.
    Java_PaymentAppServiceCallback_onInstalledPaymentHandlerFound(
        env, jcallback, app_info.second->registration_id,
        url::GURLAndroid::FromNativeGURL(env, app_info.second->scope),
        app_info.second->name.empty()
            ? nullptr
            : ConvertUTF8ToJavaString(env, app_info.second->name),
        nullptr,
        app_info.second->icon == nullptr
            ? nullptr
            : gfx::ConvertToJavaBitmap(app_info.second->icon.get()),
        ToJavaArrayOfStrings(env, app_info.second->enabled_methods),
        app_info.second->has_explicitly_verified_methods, jcapabilities,
        ToJavaArrayOfStrings(env, preferred_related_application_ids),
        jsupported_delegations);
  }

  for (const auto& installable_app : installable_apps) {
    base::android::ScopedJavaLocalRef<jobject> jsupported_delegations =
        Java_PaymentAppServiceBridge_createSupportedDelegations(
            env, installable_app.second->supported_delegations.shipping_address,
            installable_app.second->supported_delegations.payer_name,
            installable_app.second->supported_delegations.payer_phone,
            installable_app.second->supported_delegations.payer_email);

    Java_PaymentAppServiceCallback_onInstallablePaymentHandlerFound(
        env, jcallback,
        ConvertUTF8ToJavaString(env, installable_app.second->name),
        url::GURLAndroid::FromNativeGURL(
            env, GURL(installable_app.second->sw_js_url)),
        url::GURLAndroid::FromNativeGURL(
            env, GURL(installable_app.second->sw_scope)),
        installable_app.second->sw_use_cache,
        installable_app.second->icon == nullptr
            ? nullptr
            : gfx::ConvertToJavaBitmap(installable_app.second->icon.get()),
        ConvertUTF8ToJavaString(env, installable_app.first.spec()),
        ToJavaArrayOfStrings(env, installable_app.second->preferred_app_ids),
        jsupported_delegations);
  }
}

void OnPaymentAppCreationError(const JavaRef<jobject>& jcallback,
                               const std::string& error_message) {
  JNIEnv* env = AttachCurrentThread();
  Java_PaymentAppServiceCallback_onPaymentAppCreationError(
      env, jcallback, ConvertUTF8ToJavaString(env, error_message));
}

void OnDoneCreatingPaymentApps(const JavaRef<jobject>& jcallback) {
  JNIEnv* env = AttachCurrentThread();
  Java_PaymentAppServiceCallback_onDoneCreatingPaymentApps(env, jcallback);
}

}  // namespace

/* static */
void JNI_PaymentAppServiceBridge_Create(
    JNIEnv* env,
    const JavaParamRef<jobject>& jrender_frame_host,
    const JavaParamRef<jstring>& jtop_origin,
    const JavaParamRef<jobjectArray>& jmethod_data,
    jboolean jmay_crawl_for_installable_payment_apps,
    const JavaParamRef<jobject>& jcallback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* render_frame_host =
      content::RenderFrameHost::FromJavaRenderFrameHost(jrender_frame_host);
  std::string top_origin = ConvertJavaStringToUTF8(jtop_origin);

  std::vector<PaymentMethodDataPtr> method_data;
  bool success =
      DeserializeFromJavaByteBufferArray(env, jmethod_data, &method_data);
  DCHECK(success);

  scoped_refptr<payments::PaymentManifestWebDataService> web_data_service =
      WebDataServiceFactory::GetPaymentManifestWebDataForProfile(
          Profile::FromBrowserContext(
              content::WebContents::FromRenderFrameHost(render_frame_host)
                  ->GetBrowserContext()),
          ServiceAccessType::EXPLICIT_ACCESS);

  payments::PaymentAppService* service =
      GetPaymentAppService(render_frame_host);
  auto* bridge = payments::PaymentAppServiceBridge::Create(
      service->GetNumberOfFactories(), render_frame_host, GURL(top_origin),
      std::move(method_data), web_data_service,
      jmay_crawl_for_installable_payment_apps,
      base::BindRepeating(&OnPaymentAppsCreated,
                          ScopedJavaGlobalRef<jobject>(env, jcallback)),
      base::BindRepeating(&OnPaymentAppCreationError,
                          ScopedJavaGlobalRef<jobject>(env, jcallback)),
      base::BindOnce(&OnDoneCreatingPaymentApps,
                     ScopedJavaGlobalRef<jobject>(env, jcallback)));

  service->Create(bridge->GetWeakPtr());
}

namespace payments {
namespace {

// A singleton class to maintain  ownership of PaymentAppServiceBridge objects
// until Remove() is called.
class PaymentAppServiceBridgeStorage {
 public:
  static PaymentAppServiceBridgeStorage* GetInstance() {
    return base::Singleton<PaymentAppServiceBridgeStorage>::get();
  }

  PaymentAppServiceBridge* Add(std::unique_ptr<PaymentAppServiceBridge> owned) {
    DCHECK(owned);
    PaymentAppServiceBridge* key = owned.get();
    owner_[key] = std::move(owned);
    return key;
  }

  void Remove(PaymentAppServiceBridge* owned) {
    size_t number_of_deleted_objects = owner_.erase(owned);
    DCHECK_EQ(1U, number_of_deleted_objects);
  }

 private:
  friend struct base::DefaultSingletonTraits<PaymentAppServiceBridgeStorage>;
  PaymentAppServiceBridgeStorage() = default;
  ~PaymentAppServiceBridgeStorage() = default;

  std::map<PaymentAppServiceBridge*, std::unique_ptr<PaymentAppServiceBridge>>
      owner_;
};

}  // namespace

/* static */
PaymentAppServiceBridge* PaymentAppServiceBridge::Create(
    size_t number_of_factories,
    content::RenderFrameHost* render_frame_host,
    const GURL& top_origin,
    std::vector<mojom::PaymentMethodDataPtr> request_method_data,
    scoped_refptr<PaymentManifestWebDataService> web_data_service,
    bool may_crawl_for_installable_payment_apps,
    PaymentAppsCreatedCallback payment_apps_created_callback,
    PaymentAppCreationErrorCallback payment_app_creation_error_callback,
    base::OnceClosure done_creating_payment_apps_callback) {
  std::unique_ptr<PaymentAppServiceBridge> bridge(new PaymentAppServiceBridge(
      number_of_factories, render_frame_host, top_origin,
      std::move(request_method_data), std::move(web_data_service),
      may_crawl_for_installable_payment_apps,
      std::move(payment_apps_created_callback),
      std::move(payment_app_creation_error_callback),
      std::move(done_creating_payment_apps_callback)));
  return PaymentAppServiceBridgeStorage::GetInstance()->Add(std::move(bridge));
}

PaymentAppServiceBridge::PaymentAppServiceBridge(
    size_t number_of_factories,
    content::RenderFrameHost* render_frame_host,
    const GURL& top_origin,
    std::vector<mojom::PaymentMethodDataPtr> request_method_data,
    scoped_refptr<PaymentManifestWebDataService> web_data_service,
    bool may_crawl_for_installable_payment_apps,
    PaymentAppsCreatedCallback payment_apps_created_callback,
    PaymentAppCreationErrorCallback payment_app_creation_error_callback,
    base::OnceClosure done_creating_payment_apps_callback)
    : number_of_pending_factories_(number_of_factories),
      web_contents_(
          content::WebContents::FromRenderFrameHost(render_frame_host)),
      render_frame_host_(render_frame_host),
      top_origin_(top_origin),
      frame_origin_(url_formatter::FormatUrlForSecurityDisplay(
          render_frame_host->GetLastCommittedURL())),
      frame_security_origin_(render_frame_host->GetLastCommittedOrigin()),
      request_method_data_(std::move(request_method_data)),
      payment_manifest_web_data_service_(web_data_service),
      may_crawl_for_installable_payment_apps_(
          may_crawl_for_installable_payment_apps),
      payment_apps_created_callback_(std::move(payment_apps_created_callback)),
      payment_app_creation_error_callback_(
          std::move(payment_app_creation_error_callback)),
      done_creating_payment_apps_callback_(
          std::move(done_creating_payment_apps_callback)) {}

PaymentAppServiceBridge::~PaymentAppServiceBridge() = default;

base::WeakPtr<PaymentAppServiceBridge> PaymentAppServiceBridge::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

content::WebContents* PaymentAppServiceBridge::GetWebContents() {
  return web_contents_;
}
const GURL& PaymentAppServiceBridge::GetTopOrigin() {
  return top_origin_;
}

const GURL& PaymentAppServiceBridge::GetFrameOrigin() {
  return frame_origin_;
}

const url::Origin& PaymentAppServiceBridge::GetFrameSecurityOrigin() {
  return frame_security_origin_;
}

content::RenderFrameHost* PaymentAppServiceBridge::GetInitiatorRenderFrameHost()
    const {
  return render_frame_host_;
}

const std::vector<mojom::PaymentMethodDataPtr>&
PaymentAppServiceBridge::GetMethodData() const {
  return request_method_data_;
}

scoped_refptr<PaymentManifestWebDataService>
PaymentAppServiceBridge::GetPaymentManifestWebDataService() const {
  return payment_manifest_web_data_service_;
}

bool PaymentAppServiceBridge::MayCrawlForInstallablePaymentApps() {
  return may_crawl_for_installable_payment_apps_;
}

const std::vector<autofill::AutofillProfile*>&
PaymentAppServiceBridge::GetBillingProfiles() {
  NOTREACHED();
  return dummy_profiles_;
}

bool PaymentAppServiceBridge::IsRequestedAutofillDataAvailable() {
  // PaymentAppService flow should have short-circuited before this point.
  NOTREACHED();
  return false;
}

ContentPaymentRequestDelegate*
PaymentAppServiceBridge::GetPaymentRequestDelegate() const {
  NOTREACHED();
  return nullptr;
}

PaymentRequestSpec* PaymentAppServiceBridge::GetSpec() const {
  NOTREACHED();
  return nullptr;
}

void PaymentAppServiceBridge::OnPaymentAppInstalled(const url::Origin& origin,
                                                    int64_t registration_id) {
  // PaymentAppService flow should have short-circuited before this point.
  NOTREACHED();
}

void PaymentAppServiceBridge::OnPaymentAppCreated(
    std::unique_ptr<PaymentApp> app) {
  // PaymentAppService flow should have short-circuited before this point.
  NOTREACHED();
}

bool PaymentAppServiceBridge::SkipCreatingNativePaymentApps() const {
  return true;
}

void PaymentAppServiceBridge::OnCreatingNativePaymentAppsSkipped(
    content::PaymentAppProvider::PaymentApps apps,
    ServiceWorkerPaymentAppFinder::InstallablePaymentApps installable_apps) {
  payment_apps_created_callback_.Run(apps, installable_apps);
}

void PaymentAppServiceBridge::OnPaymentAppCreationError(
    const std::string& error_message) {
  payment_app_creation_error_callback_.Run(error_message);
}

void PaymentAppServiceBridge::OnDoneCreatingPaymentApps() {
  if (number_of_pending_factories_ > 1U) {
    number_of_pending_factories_--;
    return;
  }

  DCHECK_EQ(1U, number_of_pending_factories_);
  std::move(done_creating_payment_apps_callback_).Run();
  PaymentAppServiceBridgeStorage::GetInstance()->Remove(this);
}

}  // namespace payments
