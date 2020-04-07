// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

/** @fileoverview Test utils for Settings page tests. */

cr.define('settings_page_test_util', function() {
  /**
   * @param {string} type The settings page type, e.g. 'about' or 'basic'.
   * @return {!PolymerElement} The PolymerElement for the page.
   */
  /* #export */ function getPage(type) {
    const settingsUi = document.querySelector('settings-ui');
    assertTrue(!!settingsUi);
    const settingsMain = settingsUi.$$('settings-main');
    assertTrue(!!settingsMain);
    const pageType = 'settings-' + type + '-page';
    const page = settingsMain.$$(pageType);

    const idleRender = page && page.$$('settings-idle-load');
    if (!idleRender) {
      return Promise.resolve(page);
    }

    return idleRender.get().then(function() {
      Polymer.dom.flush();
      return page;
    });
  }

  /**
   * @param {!PolymerElement} page The PolymerElement for the page containing
   *     |section|.
   * @param {string} section The settings page section, e.g. 'appearance'.
   * @return {Node|undefined} The DOM node for the section.
   */
  /* #export */ function getSection(page, section) {
    const sections = page.shadowRoot.querySelectorAll('settings-section');
    assertTrue(!!sections);
    for (let i = 0; i < sections.length; ++i) {
      const s = sections[i];
      if (s.section == section) {
        return s;
      }
    }
    return undefined;
  }

  // #cr_define_end
  return {
    getPage: getPage,
    getSection: getSection,
  };
});
