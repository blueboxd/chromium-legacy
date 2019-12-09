// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'OobeI18nBehavior' is extended I18nBehavior with automatic locale change
 * propagation for children.
 */

/** @polymerBehavior */
const OobeI18nBehaviorImpl = {
  ready: function() {
    this.classList.add('i18n-dynamic');
  },

  i18nUpdateLocale: function() {
    // TODO(crbug.com/893934): move i18nUpdateLocale from I18nBehavior to this
    // class.
    I18nBehavior.i18nUpdateLocale.call(this);
    var matches = Polymer.dom(this.root).querySelectorAll('.i18n-dynamic');
    for (var child of matches) {
      if (typeof (child.i18nUpdateLocale) === 'function') {
        child.i18nUpdateLocale();
      }
    }
  },
};

/**
 * TODO: Replace with an interface. b/24294625
 * @typedef {{
 *   i18nUpdateLocale: function()
 * }}
 */
OobeI18nBehaviorImpl.Proto;
/** @polymerBehavior */
/* #export */ const OobeI18nBehavior = [I18nBehavior, OobeI18nBehaviorImpl];
