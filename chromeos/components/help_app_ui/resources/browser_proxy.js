// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const help_app = {
  handler: new helpAppUi.mojom.PageHandlerRemote()
};

// Set up a page handler to talk to the browser process.
helpAppUi.mojom.PageHandlerFactory.getRemote().createPageHandler(
    help_app.handler.$.bindNewPipeAndPassReceiver());

const GUEST_ORIGIN = 'chrome-untrusted://help-app';

/**
 * Handles messages from the untrusted context.
 * @param {Event} event
 */
function receiveMessage(event) {
  const msgEvent = /** @type{MessageEvent<string>} */ (event);
  if (msgEvent.origin !== GUEST_ORIGIN) {
    return;
  }

  switch (msgEvent.data) {
    case 'feedback':
      help_app.handler.openFeedbackDialog().then(response => {
        const guest = /** @type{HTMLIFrameElement} */ (
            document.querySelector(`iframe[src^="${GUEST_ORIGIN}"]`));
        guest.contentWindow.postMessage(response, GUEST_ORIGIN);
      });
  }
}
window.addEventListener('message', receiveMessage, false);
