// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const help_app = {
  handler: new helpAppUi.mojom.PageHandlerRemote()
};

// Set up a page handler to talk to the browser process.
helpAppUi.mojom.PageHandlerFactory.getRemote().createPageHandler(
    help_app.handler.$.bindNewPipeAndPassReceiver());
