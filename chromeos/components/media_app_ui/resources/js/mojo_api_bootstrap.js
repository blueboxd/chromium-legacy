// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const media_app = {
  callbackRouter: new mediaAppUi.mojom.PageCallbackRouter(),
  handler: new mediaAppUi.mojom.PageHandlerRemote()
};

// Get the PageHandlerFactory remote and call createPageHandler.
// This bootstraps the mojom connection by getting the CPP to set up a
// handler for our future calls.
mediaAppUi.mojom.PageHandlerFactory.getRemote().createPageHandler(
    media_app.callbackRouter.$.bindNewPipeAndPassRemote(),
    media_app.handler.$.bindNewPipeAndPassReceiver());
