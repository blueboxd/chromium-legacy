// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/application_controller_impl.h"

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"

ApplicationControllerImpl::ApplicationControllerImpl(
    fuchsia::web::Frame* frame,
    fidl::InterfaceHandle<chromium::cast::ApplicationContext> context)
    : binding_(this), frame_(frame) {
  DCHECK(context);
  DCHECK(frame_);

  context.Bind()->SetApplicationController(binding_.NewBinding());

  binding_.set_error_handler([](zx_status_t status) {
    if (status != ZX_ERR_PEER_CLOSED && status != ZX_ERR_CANCELED) {
      ZX_LOG(WARNING, status) << "Application bindings connection dropped.";
    }
  });
}

ApplicationControllerImpl::~ApplicationControllerImpl() = default;

void ApplicationControllerImpl::SetTouchInputEnabled(bool enable) {
  frame_->ConfigureInputTypes(fuchsia::web::InputTypes::GESTURE_TAP |
                                  fuchsia::web::InputTypes::GESTURE_DRAG,
                              (enable ? fuchsia::web::AllowInputState::ALLOW
                                      : fuchsia::web::AllowInputState::DENY));
}
