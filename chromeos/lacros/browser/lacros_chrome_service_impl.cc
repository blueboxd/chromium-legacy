// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/lacros/browser/lacros_chrome_service_impl.h"

namespace chromeos {

LacrosChromeServiceImpl::LacrosChromeServiceImpl() {
  // TODO(hidehiko): Remove non-error logging from here.
  // Currently, LacrosChromeService interface is empty, so
  // this is only way to make sure the connection is established.
  LOG(WARNING) << "LacrosChromeService is connected";
}

LacrosChromeServiceImpl::~LacrosChromeServiceImpl() = default;

}  // namespace chromeos
