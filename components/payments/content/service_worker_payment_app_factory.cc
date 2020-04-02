// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/service_worker_payment_app_factory.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/content_payment_request_delegate.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/service_worker_payment_app.h"
#include "components/payments/content/service_worker_payment_app_finder.h"
#include "content/public/browser/web_contents.h"

namespace payments {
namespace {

std::vector<mojom::PaymentMethodDataPtr> Clone(
    const std::vector<mojom::PaymentMethodDataPtr>& original) {
  std::vector<mojom::PaymentMethodDataPtr> clone(original.size());
  std::transform(
      original.begin(), original.end(), clone.begin(),
      [](const mojom::PaymentMethodDataPtr& item) { return item.Clone(); });
  return clone;
}

}  // namespace

class ServiceWorkerPaymentAppCreator {
 public:
  ServiceWorkerPaymentAppCreator(
      ServiceWorkerPaymentAppFactory* owner,
      base::WeakPtr<PaymentAppFactory::Delegate> delegate)
      : owner_(owner), delegate_(delegate) {}

  ~ServiceWorkerPaymentAppCreator() {}

  void CreatePaymentApps(
      content::PaymentAppProvider::PaymentApps apps,
      ServiceWorkerPaymentAppFinder::InstallablePaymentApps installable_apps,
      const std::string& error_message) {
    if (!delegate_) {
      FinishAndCleanup();
      return;
    }

    if (!error_message.empty())
      delegate_->OnPaymentAppCreationError(error_message);

    number_of_pending_sw_payment_apps_ = apps.size() + installable_apps.size();
    if (number_of_pending_sw_payment_apps_ == 0U) {
      FinishAndCleanup();
      return;
    }

    if (delegate_->SkipCreatingNativePaymentApps()) {
      delegate_->OnCreatingNativePaymentAppsSkipped(
          std::move(apps), std::move(installable_apps));
      FinishAndCleanup();
      return;
    }

    for (auto& installed_app : apps) {
      auto app = std::make_unique<ServiceWorkerPaymentApp>(
          delegate_->GetWebContents()->GetBrowserContext(),
          delegate_->GetTopOrigin(), delegate_->GetFrameOrigin(),
          delegate_->GetSpec(), std::move(installed_app.second),
          delegate_->GetPaymentRequestDelegate(),
          base::BindRepeating(
              &PaymentAppFactory::Delegate::OnPaymentAppInstalled, delegate_));
      app->ValidateCanMakePayment(base::BindOnce(
          &ServiceWorkerPaymentAppCreator::OnSWPaymentAppValidated,
          weak_ptr_factory_.GetWeakPtr()));
      PaymentApp* raw_payment_app_pointer = app.get();
      available_apps_[raw_payment_app_pointer] = std::move(app);
    }

    for (auto& installable_app : installable_apps) {
      auto app = std::make_unique<ServiceWorkerPaymentApp>(
          delegate_->GetWebContents(), delegate_->GetTopOrigin(),
          delegate_->GetFrameOrigin(), delegate_->GetSpec(),
          std::move(installable_app.second), installable_app.first.spec(),
          delegate_->GetPaymentRequestDelegate(),
          base::BindRepeating(
              &PaymentAppFactory::Delegate::OnPaymentAppInstalled, delegate_));
      app->ValidateCanMakePayment(base::BindOnce(
          &ServiceWorkerPaymentAppCreator::OnSWPaymentAppValidated,
          weak_ptr_factory_.GetWeakPtr()));
      PaymentApp* raw_payment_app_pointer = app.get();
      available_apps_[raw_payment_app_pointer] = std::move(app);
    }
  }

  base::WeakPtr<ServiceWorkerPaymentAppCreator> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void OnSWPaymentAppValidated(ServiceWorkerPaymentApp* app, bool result) {
    if (!delegate_) {
      FinishAndCleanup();
      return;
    }

    auto iterator = available_apps_.find(app);
    if (iterator != available_apps_.end()) {
      if (result)
        delegate_->OnPaymentAppCreated(std::move(iterator->second));
      available_apps_.erase(iterator);
    }

    if (--number_of_pending_sw_payment_apps_ == 0)
      FinishAndCleanup();
  }

  void FinishAndCleanup() {
    if (delegate_)
      delegate_->OnDoneCreatingPaymentApps();
    owner_->DeleteCreator(this);
  }

  ServiceWorkerPaymentAppFactory* owner_;
  base::WeakPtr<PaymentAppFactory::Delegate> delegate_;
  std::map<PaymentApp*, std::unique_ptr<PaymentApp>> available_apps_;
  int number_of_pending_sw_payment_apps_ = 0;

  base::WeakPtrFactory<ServiceWorkerPaymentAppCreator> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerPaymentAppCreator);
};

ServiceWorkerPaymentAppFactory::ServiceWorkerPaymentAppFactory()
    : PaymentAppFactory(PaymentApp::Type::SERVICE_WORKER_APP) {}

ServiceWorkerPaymentAppFactory::~ServiceWorkerPaymentAppFactory() {}

void ServiceWorkerPaymentAppFactory::Create(base::WeakPtr<Delegate> delegate) {
  DCHECK(delegate->GetWebContents());
  auto creator = std::make_unique<ServiceWorkerPaymentAppCreator>(
      /*owner=*/this, delegate);
  ServiceWorkerPaymentAppCreator* creator_raw_pointer = creator.get();
  creators_[creator_raw_pointer] = std::move(creator);

  ServiceWorkerPaymentAppFinder::GetInstance()->GetAllPaymentApps(
      delegate->GetFrameSecurityOrigin(),
      delegate->GetInitiatorRenderFrameHost(), delegate->GetWebContents(),
      delegate->GetPaymentManifestWebDataService(),
      Clone(delegate->GetMethodData()),
      delegate->MayCrawlForInstallablePaymentApps(),
      base::BindOnce(&ServiceWorkerPaymentAppCreator::CreatePaymentApps,
                     creator_raw_pointer->GetWeakPtr()),
      base::BindOnce([]() {
        // Nothing needs to be done after writing cache. This callback is used
        // only in tests.
      }));
}

void ServiceWorkerPaymentAppFactory::DeleteCreator(
    ServiceWorkerPaymentAppCreator* creator_raw_pointer) {
  size_t number_of_deleted_creators = creators_.erase(creator_raw_pointer);
  DCHECK_EQ(1U, number_of_deleted_creators);
}

}  // namespace payments
