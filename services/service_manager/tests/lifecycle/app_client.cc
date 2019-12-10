// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/tests/lifecycle/app_client.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "services/service_manager/public/cpp/service_binding.h"

namespace service_manager {
namespace test {

AppClient::AppClient(service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)) {
  receivers_.set_disconnect_handler(base::BindRepeating(
      &AppClient::LifecycleControlBindingLost, base::Unretained(this)));

  registry_.AddInterface<mojom::LifecycleControl>(
      base::BindRepeating(&AppClient::Create, base::Unretained(this)));
}

AppClient::~AppClient() = default;

void AppClient::OnBindInterface(const BindSourceInfo& source_info,
                                const std::string& interface_name,
                                mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

void AppClient::OnDisconnected() {
  DCHECK(service_binding_.is_bound());
  service_binding_.Close();
  Terminate();
}

void AppClient::Create(
    mojo::PendingReceiver<mojom::LifecycleControl> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void AppClient::Ping(PingCallback callback) {
  std::move(callback).Run();
}

void AppClient::GracefulQuit() {
  if (service_binding_.is_bound())
    service_binding_.RequestClose();
  else
    Terminate();
}

void AppClient::Crash() {
  // Rather than actually crash, which causes a bunch of console spray and
  // maybe UI clutter on some platforms, just exit without shutting anything
  // down properly.
  exit(1);
}

void AppClient::CloseServiceManagerConnection() {
  if (service_binding_.is_bound())
    service_binding_.Close();
}

void AppClient::LifecycleControlBindingLost() {
  if (!service_binding_.is_bound() && receivers_.empty())
    Terminate();
}

}  // namespace test
}  // namespace service_manager
