// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/controller_service_worker_connector.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace content {

ControllerServiceWorkerConnector::ControllerServiceWorkerConnector(
    blink::mojom::ServiceWorkerContainerHostPtrInfo container_host_info,
    mojo::PendingRemote<blink::mojom::ControllerServiceWorker>
        remote_controller,
    const std::string& client_id)
    : client_id_(client_id) {
  container_host_ptr_.Bind(std::move(container_host_info));
  container_host_ptr_.set_connection_error_handler(base::BindOnce(
      &ControllerServiceWorkerConnector::OnContainerHostConnectionClosed,
      base::Unretained(this)));
  SetControllerServiceWorker(std::move(remote_controller));
}

blink::mojom::ControllerServiceWorker*
ControllerServiceWorkerConnector::GetControllerServiceWorker(
    blink::mojom::ControllerServiceWorkerPurpose purpose) {
  switch (state_) {
    case State::kDisconnected: {
      DCHECK(!controller_service_worker_);
      DCHECK(container_host_ptr_);
      mojo::PendingRemote<blink::mojom::ControllerServiceWorker>
          remote_controller;

      container_host_ptr_->EnsureControllerServiceWorker(
          remote_controller.InitWithNewPipeAndPassReceiver(), purpose);

      SetControllerServiceWorker(std::move(remote_controller));
      return controller_service_worker_.get();
    }
    case State::kConnected:
      DCHECK(controller_service_worker_.is_bound());
      return controller_service_worker_.get();
    case State::kNoController:
      DCHECK(!controller_service_worker_);
      return nullptr;
    case State::kNoContainerHost:
      DCHECK(!controller_service_worker_);
      DCHECK(!container_host_ptr_);
      return nullptr;
  }
  NOTREACHED();
  return nullptr;
}

void ControllerServiceWorkerConnector::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ControllerServiceWorkerConnector::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ControllerServiceWorkerConnector::OnContainerHostConnectionClosed() {
  state_ = State::kNoContainerHost;
  container_host_ptr_.reset();
  controller_service_worker_.reset();
}

void ControllerServiceWorkerConnector::OnControllerConnectionClosed() {
  DCHECK_EQ(State::kConnected, state_);
  state_ = State::kDisconnected;
  controller_service_worker_.reset();
  for (auto& observer : observer_list_)
    observer.OnConnectionClosed();
}

void ControllerServiceWorkerConnector::AddBinding(
    blink::mojom::ControllerServiceWorkerConnectorRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void ControllerServiceWorkerConnector::UpdateController(
    mojo::PendingRemote<blink::mojom::ControllerServiceWorker> controller) {
  if (state_ == State::kNoContainerHost)
    return;
  SetControllerServiceWorker(std::move(controller));
  if (!controller_service_worker_)
    state_ = State::kNoController;
}

void ControllerServiceWorkerConnector::SetControllerServiceWorker(
    mojo::PendingRemote<blink::mojom::ControllerServiceWorker> controller) {
  controller_service_worker_.reset();
  if (!controller)
    return;
  controller_service_worker_.Bind(std::move(controller));
  if (controller_service_worker_) {
    controller_service_worker_.set_disconnect_handler(base::BindOnce(
        &ControllerServiceWorkerConnector::OnControllerConnectionClosed,
        base::Unretained(this)));
    state_ = State::kConnected;
  }
}

ControllerServiceWorkerConnector::~ControllerServiceWorkerConnector() = default;

}  // namespace content
