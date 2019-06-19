// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_next_home/kiosk_next_home_interface_broker_impl.h"

#include <utility>

#include "chrome/browser/chromeos/kiosk_next_home/app_controller_service.h"
#include "chrome/browser/chromeos/kiosk_next_home/identity_controller_impl.h"
#include "chrome/browser/chromeos/kiosk_next_home/website_controller_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "services/identity/public/mojom/constants.mojom.h"
#include "services/identity/public/mojom/identity_accessor.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {
namespace kiosk_next_home {

KioskNextHomeInterfaceBrokerImpl::KioskNextHomeInterfaceBrokerImpl(
    content::BrowserContext* context)
    : context_(context),
      connector_(content::BrowserContext::GetConnectorFor(context)->Clone()) {}

KioskNextHomeInterfaceBrokerImpl::~KioskNextHomeInterfaceBrokerImpl() = default;

void KioskNextHomeInterfaceBrokerImpl::GetIdentityAccessor(
    ::identity::mojom::IdentityAccessorRequest request) {
  connector_->BindInterface(::identity::mojom::kServiceName,
                            std::move(request));
}

void KioskNextHomeInterfaceBrokerImpl::GetIdentityController(
    mojom::IdentityControllerRequest request) {
  identity_controller_ =
      std::make_unique<IdentityControllerImpl>(std::move(request));
}

void KioskNextHomeInterfaceBrokerImpl::GetAppController(
    mojom::AppControllerRequest request) {
  AppControllerService::Get(context_)->BindRequest(std::move(request));
}

void KioskNextHomeInterfaceBrokerImpl::GetWebsiteController(
    mojom::WebsiteControllerRequest request) {
  website_controller_ =
      std::make_unique<WebsiteControllerImpl>(std::move(request));
}

void KioskNextHomeInterfaceBrokerImpl::BindRequest(
    mojom::KioskNextHomeInterfaceBrokerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace kiosk_next_home
}  // namespace chromeos
