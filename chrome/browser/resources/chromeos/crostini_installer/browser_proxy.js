// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('crostini_installer', function() {
  class BrowserProxy {
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

  cr.addSingletonGetter(BrowserProxy);

  return {BrowserProxy: BrowserProxy};
});
