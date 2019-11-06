// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements the registration of guestview elements when
// permissions are not available. These elements exist only to provide a useful
// error message when developers attempt to use them.

var $CustomElementRegistry =
    require('safeMethods').SafeMethods.$CustomElementRegistry;
var $EventTarget = require('safeMethods').SafeMethods.$EventTarget;
var GuestViewInternalNatives = requireNative('guest_view_internal');

function registerDeniedElementInternal(viewType, permissionName) {
  GuestViewInternalNatives.AllowGuestViewElementDefinition(() => {
    var DeniedElement = class extends HTMLElement {
      constructor() {
        super();
        window.console.error(`You do not have permission to use the ${
            viewType} element. Be sure to declare the "${
            permissionName}" permission in your manifest file.`);
      }
    }
    $CustomElementRegistry.define(
        window.customElements, $String.toLowerCase(viewType), DeniedElement);
    // User code that does not have permission for this GuestView could be
    // using the same name for another purpose, in which case we won't overwrite
    // with the error-providing element.
    if (!$Object.hasOwnProperty(window, viewType)) {
      $Object.defineProperty(window, viewType, {
        value: DeniedElement,
      });
    }
  });
}

// Registers an error-providing GuestView custom element.
function registerDeniedElement(viewType, permissionName) {
  let useCapture = true;
  window.addEventListener('readystatechange', function listener(event) {
    if (document.readyState == 'loading')
      return;

    registerDeniedElementInternal(viewType, permissionName);

    $EventTarget.removeEventListener(window, event.type, listener, useCapture);
  }, useCapture);
}

// Exports.
exports.$set('registerDeniedElement', registerDeniedElement);
