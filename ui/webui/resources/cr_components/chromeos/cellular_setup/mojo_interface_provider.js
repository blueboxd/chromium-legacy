// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

cr.define('cellular_setup', function() {
  /** @interface */
  /* #export */ class MojoInterfaceProvider {
    /** @return {!chromeos.cellularSetup.mojom.CellularSetupRemote} */
    getMojoServiceRemote() {}
  }

  /** @implements {cellular_setup.MojoInterfaceProvider} */
  /* #export */ class MojoInterfaceProviderImpl {
    constructor() {
      /** @private {?chromeos.cellularSetup.mojom.CellularSetupRemote} */
      this.remote_ = null;
    }

    /** @override */
    getMojoServiceRemote() {
      if (!this.remote_) {
        this.remote_ = chromeos.cellularSetup.mojom.CellularSetup.getRemote();
      }

      return this.remote_;
    }
  }

  cr.addSingletonGetter(MojoInterfaceProviderImpl);

  // #cr_define_end
  return {
    MojoInterfaceProvider: MojoInterfaceProvider,
    MojoInterfaceProviderImpl: MojoInterfaceProviderImpl,
  };
});
