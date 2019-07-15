// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  class BrowserProxy {
    constructor() {
      /** @type {downloads.mojom.PageCallbackRouter} */
      this.callbackRouter = new downloads.mojom.PageCallbackRouter();

      /** @type {downloads.mojom.PageHandlerRemote} */
      this.handler = new downloads.mojom.PageHandlerRemote();

      const factory = downloads.mojom.PageHandlerFactory.getRemote();
      factory.createPageHandler(
          this.callbackRouter.$.bindNewPipeAndPassRemote(),
          this.handler.$.bindNewPipeAndPassReceiver());
    }
  }

  cr.addSingletonGetter(BrowserProxy);

  return {BrowserProxy: BrowserProxy};
});
