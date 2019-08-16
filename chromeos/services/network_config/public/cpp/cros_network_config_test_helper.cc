// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"

#include "chromeos/network/network_device_handler.h"
#include "chromeos/services/network_config/cros_network_config.h"
#include "chromeos/services/network_config/in_process_instance.h"

namespace chromeos {
namespace network_config {

CrosNetworkConfigTestHelper::CrosNetworkConfigTestHelper() {
  network_device_handler_ =
      chromeos::NetworkDeviceHandler::InitializeForTesting(
          network_state_helper_.network_state_handler());
  cros_network_config_impl_ =
      std::make_unique<chromeos::network_config::CrosNetworkConfig>(
          network_state_helper_.network_state_handler(),
          network_device_handler_.get(),
          /*network_configuration_handler=*/nullptr);
  OverrideInProcessInstanceForTesting(cros_network_config_impl_.get());
}

CrosNetworkConfigTestHelper::~CrosNetworkConfigTestHelper() {
  OverrideInProcessInstanceForTesting(nullptr);
}

}  // namespace network_config
}  // namespace chromeos
