// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/identity_service.h"

#include <utility>

#include "base/bind.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/identity/identity_accessor_impl.h"

namespace identity {

IdentityService::IdentityService(signin::IdentityManager* identity_manager,
                                 service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)),
      identity_manager_(identity_manager) {
  registry_.AddInterface<mojom::IdentityAccessor>(
      base::Bind(&IdentityService::Create, base::Unretained(this)));
}

IdentityService::~IdentityService() {
  ShutDown();
}

void IdentityService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

void IdentityService::ShutDown() {
  if (IsShutDown())
    return;

  identity_manager_ = nullptr;
  identity_accessor_bindings_.CloseAllBindings();
}

bool IdentityService::IsShutDown() {
  return (identity_manager_ == nullptr);
}

void IdentityService::Create(mojom::IdentityAccessorRequest request) {
  // This instance cannot service requests if it has already been shut down.
  if (IsShutDown())
    return;

  identity_accessor_bindings_.AddBinding(
      std::make_unique<IdentityAccessorImpl>(identity_manager_),
      std::move(request));
}

}  // namespace identity
