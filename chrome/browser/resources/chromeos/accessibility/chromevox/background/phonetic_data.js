// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides phonetic disambiguation functionality across multiple
 * languages for ChromeVox.
 *
 */

goog.provide('PhoneticData');

goog.require('JaPhoneticData');

/**
 * Initialization function for PhoneticData.
 */
PhoneticData.init = function() {
  JaPhoneticData.init();
};

/**
 * Returns the phonetic disambiguation for |character| in |locale|.
 * Returns empty string if disambiguation can't be found.
 * @param {string} locale
 * @param {string} character
 * @return {string}
 */
PhoneticData.getPhoneticDisambiguation = function(locale, character) {
  const phoneticDictionaries =
      chrome.extension.getBackgroundPage().PhoneticDictionaries;
  if (!locale || !character || !phoneticDictionaries ||
      !phoneticDictionaries.phoneticMap_) {
    return '';
  }

  locale = locale.toLowerCase();
  character = character.toLowerCase();
  let map = null;
  if (locale === 'ja') {
    map = JaPhoneticData.phoneticMap_;
  } else {
    // Try a lookup using |locale|, but use only the language component if the
    // lookup fails, e.g. "en-us" -> "en" or "zh-hant-hk" -> "zh".
    map = phoneticDictionaries.phoneticMap_[locale] ||
        phoneticDictionaries.phoneticMap_[locale.split('-')[0]];
  }

  if (!map) {
    return '';
  }

  return map[character] || '';
};
