// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @file Utilities for form-associated custom elements
 */

import * as reflection from '../internal/reflection.mjs';

function installGetter(proto, propName, getter) {
  Object.defineProperty(
      getter, 'name',
      {configurable: true, enumerable: false, value: 'get ' + propName});
  Object.defineProperty(
      proto, propName, {configurable: true, enumerable: true, get: getter});
}

/**
 * Add the following properties to |proto|.
 *   - disabled
 *   - name
 *   - type
 *   - form
 *   - willValidate
 *   - validity
 *   - validationMessage
 *   - labels
 *   - checkValidity()
 *   - reportValidity()
 *   - setCustomValidity(error)
 *
 * @param {!Object} proto An Element prototype which will have properties
 * @param {!Symbol} internals A Symbol of the ElementInternals property of the
 *     element
 */
export function installPropertiesAndFunctions(proto, internals) {
  reflection.installBool(proto, 'disabled');
  reflection.installString(proto, 'name');
  installGetter(proto, 'type', function() {
    if (!(this instanceof proto.constructor)) {
      throw TypeError(
          'The context object is not an instance of ' + proto.contructor.name);
    }
    return this.localName;
  });

  installGetter(proto, 'form', function() {
    return this[internals].form;
  });
  installGetter(proto, 'willValidate', function() {
    return this[internals].willValidate;
  });
  installGetter(proto, 'validity', function() {
    return this[internals].validity;
  });
  installGetter(proto, 'validationMessage', function() {
    return this[internals].validationMessage;
  });
  installGetter(proto, 'labels', function() {
    return this[internals].labels;
  });
  proto.checkValidity = function() {
    return this[internals].checkValidity();
  };
  proto.reportValidity = function() {
    return this[internals].reportValidity();
  };
  proto.setCustomValidity = function(error) {
    if (error === undefined) {
      throw new TypeError('Too few arguments');
    }
    this[internals].setValidity({customError: true}, error);
  };
}
