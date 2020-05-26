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

function addOverride(object, descriptorObject, propertyName, getter, setter) {
  var original =
      Object.getOwnPropertyDescriptor(descriptorObject, propertyName);
  if (original && original.configurable) {
    var propertyDescriptor = {
      get: getter,
      configurable: false,
    };
    if (setter) {
      propertyDescriptor.set = setter;
    }
    Object.defineProperty(object, propertyName, propertyDescriptor);
  } else {
    // TODO(crbug.com/1082151): Track this occurrence.
  }
}

addOverride(
    document, Document.prototype, 'cookie',
    function() {
      return '';
    },
    function(value) {
      // no-op;
    });

addOverride(window, window, 'localStorage', function() {
  throw new DOMException(
      'Failed to read the \'localStorage\' property from \'window\': ' +
          'Access is denied for this document',
      'SecurityError');
}, null);

addOverride(window, window, 'sessionStorage', function() {
  throw new DOMException(
      'Failed to read the \'sessionStorage\' property from \'window\': ' +
          'Access is denied for this document',
      'SecurityError');
}, null);
}());
