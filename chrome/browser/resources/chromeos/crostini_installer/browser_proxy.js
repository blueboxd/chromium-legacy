// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(lxj): use es6 module when it is ready crbug/1004256
import './crostini_installer.mojom-lite.js';

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

export class BrowserProxy {
  constructor() {
    /** @type {chromeos.crostiniInstaller.mojom.PageCallbackRouter} */
    this.callbackRouter =
        new chromeos.crostiniInstaller.mojom.PageCallbackRouter();
    /** @type {chromeos.crostiniInstaller.mojom.PageHandlerRemote} */
    this.handler = new chromeos.crostiniInstaller.mojom.PageHandlerRemote();

    const factory =
        chromeos.crostiniInstaller.mojom.PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }
}

addSingletonGetter(BrowserProxy);
