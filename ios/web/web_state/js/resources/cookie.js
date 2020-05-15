// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Add functionality related to cookies.
 */
goog.provide('__crWeb.cookie');

/* Beginning of anonymous namespace. */
(function() {

function isCrossOriginFrame() {
  try {
    return (!window.top.location.hostname);
  } catch (e) {
    return true;
  }
}

function cookiesAllowed(state) {
  switch (state) {
    case 'enabled':
      return true;
    case 'block-third-party':
      return !isCrossOriginFrame();
    case 'block':
      return false;
    default:
      return true;
  }
}

var state = '$(COOKIE_STATE)';

if (cookiesAllowed(state)) {
  return;
}

var originalCookie =
    Object.getOwnPropertyDescriptor(Document.prototype, 'cookie');
// It should always be possible to override this descriptor unless WebKit
// changes its behavior, but if it isn't, track that.
if (!originalCookie || !originalCookie.configurable) {
  // TODO(crbug.com/1082151): Track this occurrence.
  return;
}

Object.defineProperty(document, 'cookie', {
  get: function() {
    return ''
  },
  set: function(val) {
    // No-op.
  }
});
}());
