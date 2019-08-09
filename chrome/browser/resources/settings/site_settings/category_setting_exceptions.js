// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'category-setting-exceptions' is the polymer element for showing a certain
 * category of exceptions under Site Settings.
 */
Polymer({
  is: 'category-setting-exceptions',

  properties: {

    /**
     * The string ID of the category that this element is displaying data for.
     * See site_settings/constants.js for possible values.
     * @type {!settings.ContentSettingsTypes}
     */
    category: String,

    /**
     * Some content types (like Location) do not allow the user to manually
     * edit the exception list from within Settings.
     * @private
     */
    readOnlyList: {
      type: Boolean,
      value: false,
    },

    /**
     * The heading text for the blocked exception list.
     */
    blockHeader: String,

    searchFilter: String,

    /**
     * If true, displays the Allow site list. Defaults to true.
     * @private
     */
    showAllowSiteList_: {
      type: Boolean,
      computed: 'computeShowAllowSiteList_(category)',
    },

    /**
     * If true, displays the Block site list. Defaults to true.
     */
    showBlockSiteList_: {
      type: Boolean,
      value: true,
    },
  },

  /** @override */
  ready: function() {
    this.ContentSetting = settings.ContentSetting;
  },

  /**
   * Hides particular category subtypes if |this.category| does not support the
   * content setting of that type.
   * @return {boolean}
   * @private
   */
  computeShowAllowSiteList_: function() {
    return this.category !=
        settings.ContentSettingsTypes.NATIVE_FILE_SYSTEM_WRITE;
  },
});
