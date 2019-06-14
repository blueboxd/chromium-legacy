// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of CrosNetworkConfig for testing.
 * Currently this is built on top of FakeNetworkingPrivate to avoid
 * inconsistencies between fakes in tests.
 */

// TODO(stevenjb): Include cros_network_config.mojom.js and extend
// CrosNetworkConfigInterface
class FakeNetworkConfig {
  /** @param {!NetworkingPrivate} extensionApi */
  constructor(extensionApi) {
    this.extensionApi_ = extensionApi;
  }

  /**
   * @param {!chromeos.networkConfig.mojom.NetworkType} type
   * @param {boolean} enabled
   * @return {!Promise<{success: boolean}>}
   */
  setNetworkTypeEnabledState(type, enabled) {
    if (enabled) {
      this.extensionApi_.enableNetworkType(OncMojo.getNetworkTypeString(type));
    } else {
      this.extensionApi_.disableNetworkType(OncMojo.getNetworkTypeString(type));
    }
    return Promise.resolve(true);
  }
}
