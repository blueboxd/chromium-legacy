// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/test/echo/echo_service.h"

#include "base/bind.h"

namespace echo {

EchoService::EchoService() = default;

EchoService::~EchoService() = default;

void EchoService::EchoString(const std::string& input,
                             EchoStringCallback callback) {
  std::move(callback).Run(input);
}

void EchoService::Quit() {}

}  // namespace echo
