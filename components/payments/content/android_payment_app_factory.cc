// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android_payment_app_factory.h"

#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace payments {
namespace {

class AppFinder : public base::SupportsUserData::Data {
 public:
  static base::WeakPtr<AppFinder> CreateAndSetOwnedBy(
      base::SupportsUserData* owner) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK(owner);
    auto owned = std::make_unique<AppFinder>(owner);
    auto weak_ptr = owned->weak_ptr_factory_.GetWeakPtr();
    const void* key = owned.get();
    owner->SetUserData(key, std::move(owned));
    return weak_ptr;
  }

  explicit AppFinder(base::SupportsUserData* owner) : owner_(owner) {}
  ~AppFinder() override = default;

  AppFinder(const AppFinder& other) = delete;
  AppFinder& operator=(const AppFinder& other) = delete;

  void FindApps(base::WeakPtr<PaymentAppFactory::Delegate> delegate) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK_EQ(nullptr, delegate_.get());
    DCHECK_NE(nullptr, delegate.get());

    delegate_ = delegate;

    OnDoneCreatingPaymentApps();
  }

 private:
  void OnDoneCreatingPaymentApps() {
    if (delegate_)
      delegate_->OnDoneCreatingPaymentApps();

    owner_->RemoveUserData(this);
  }

  base::SupportsUserData* owner_;
  base::WeakPtr<PaymentAppFactory::Delegate> delegate_;

  base::WeakPtrFactory<AppFinder> weak_ptr_factory_{this};
};

}  // namespace

AndroidPaymentAppFactory::AndroidPaymentAppFactory()
    : PaymentAppFactory(PaymentApp::Type::NATIVE_MOBILE_APP) {}

AndroidPaymentAppFactory::~AndroidPaymentAppFactory() = default;

void AndroidPaymentAppFactory::Create(base::WeakPtr<Delegate> delegate) {
  auto app_finder = AppFinder::CreateAndSetOwnedBy(delegate->GetWebContents());
  app_finder->FindApps(delegate);
}

}  // namespace payments
