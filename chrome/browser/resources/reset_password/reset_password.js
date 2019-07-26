/* Copyright 2018 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

'use strict';
/**
 * JavaScript for reset_password.html, served from chrome://reset-password/.
 */
(function() {

/** @type {mojom.ResetPasswordHandlerRemote} */
let pageHandler;

function initialize() {
  pageHandler = mojom.ResetPasswordHandler.getRemote();

  /** @type {?HTMLElement} */
  const resetPasswordButton = $('reset-password-button');
  resetPasswordButton.addEventListener('click', function() {
    pageHandler.handlePasswordReset();
  });
}

document.addEventListener('DOMContentLoaded', initialize);
})();
