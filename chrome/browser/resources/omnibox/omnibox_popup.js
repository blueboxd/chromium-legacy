// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import './chrome/browser/ui/webui/omnibox/omnibox.mojom-lite.js';

/**
 * Javascript proof-of-concept for omnibox_popup.html, served from
 * chrome://omnibox/omnibox_popup.html. This is used for the experimental
 * WebUI version of the omnibox popup.
 */

document.addEventListener('DOMContentLoaded', () => {
  /** @private {!mojom.OmniboxPageCallbackRouter} */
  const callbackRouter = new mojom.OmniboxPageCallbackRouter;

  // Basically a Hello World proof of concept that writes the Autocomplete
  // responses to the whole document.
  callbackRouter.handleNewAutocompleteResponse.addListener(
      (response, isPageController) => {
        // Ignore debug controller and empty results.
        if (!isPageController && response.combinedResults.length > 0) {
          const ol = document.querySelector('#omnibox-results');
          while (ol.firstChild) {
            ol.firstChild.remove();
          }
          response.combinedResults.forEach(result => {
            const li = document.createElement('li');
            li.textContent = `${result.contents} - ${result.description}`;
            ol.appendChild(li);
          });
        }
      });

  /** @private {!mojom.OmniboxPageHandlerRemote} */
  const handler = mojom.OmniboxPageHandler.getRemote();
  handler.setClientPage(callbackRouter.$.bindNewPipeAndPassRemote());
});
