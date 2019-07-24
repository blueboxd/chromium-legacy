// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/identity_service_creator.h"

#include <memory>

#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "services/identity/identity_service.h"

std::unique_ptr<service_manager::Service> CreateIdentityService(
    ios::ChromeBrowserState* browser_state,
    service_manager::mojom::ServiceRequest request) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(browser_state);
  return std::make_unique<identity::IdentityService>(identity_manager,
                                                     std::move(request));
}
