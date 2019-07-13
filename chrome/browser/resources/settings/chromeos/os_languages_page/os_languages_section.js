// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-languages-section' is the top-level settings section for
 * languages.
 */
Polymer({
  is: 'os-settings-languages-section',

  properties: {
    prefs: Object,

    /** @type {!LanguagesModel|undefined} */
    languages: {
      type: Object,
      notify: true,
    },

    /** @type {!LanguageHelper} */
    languageHelper: Object,

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value: function() {
        const map = new Map();
        if (settings.routes.LANGUAGES_DETAILS) {
          map.set(
              settings.routes.LANGUAGES_DETAILS.path,
              '#languagesSubpageTrigger .subpage-arrow');
        }
        return map;
      },
    }
  },

  /** @private */
  onLanguagesTap_: function() {
    // TODO(crbug.com/950007): Add UMA metric for opening language details.
    settings.navigateTo(settings.routes.LANGUAGES_DETAILS);
  },

  /**
   * @param {string} prospectiveUILanguage
   * @return {string}
   * @private
   */
  getLanguageDisplayName_: function(prospectiveUILanguage) {
    return this.languageHelper.getLanguage(prospectiveUILanguage).displayName;
  },

  /**
   * @param {string} id The input method ID.
   * @return {string}
   * @private
   */
  getInputMethodDisplayName_: function(id) {
    const inputMethod =
        this.languages.inputMethods.enabled.find(function(inputMethod) {
          return inputMethod.id == id;
        });
    return inputMethod ? inputMethod.displayName : '';
  },
});
