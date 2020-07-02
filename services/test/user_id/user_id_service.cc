// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/test/user_id/user_id_service.h"
#include "base/bind.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace user_id {

UserIdService::UserIdService(
    mojo::PendingReceiver<service_manager::mojom::Service> receiver)
    : service_receiver_(this, std::move(receiver)) {
  registry_.AddInterface<mojom::UserId>(base::BindRepeating(
      &UserIdService::BindUserIdReceiver, base::Unretained(this)));
}

UserIdService::~UserIdService() = default;

void UserIdService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

void UserIdService::BindUserIdReceiver(
    mojo::PendingReceiver<mojom::UserId> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void UserIdService::GetInstanceGroup(GetInstanceGroupCallback callback) {
  std::move(callback).Run(service_receiver_.identity().instance_group());
}

}  // namespace user_id
