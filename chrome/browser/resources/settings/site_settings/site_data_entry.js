// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-data-entry' handles showing the local storage summary for a site.
 */

/**
 * @typedef {{
 *   site: string,
 *   id: string,
 *   localData: string,
 * }}
 */
/* #export */ let CookieDataSummaryItem;

Polymer({
  is: 'site-data-entry',

  behaviors: [
    cr.ui.FocusRowBehavior,
    I18nBehavior,
  ],

  properties: {
    /** @type {!CookieDataSummaryItem} */
    model: Object,
  },

  /** @private {settings.LocalDataBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready() {
    this.browserProxy_ = settings.LocalDataBrowserProxyImpl.getInstance();
  },

  /**
   * Deletes all site data for this site.
   * @param {!Event} e
   * @private
   */
  onRemove_(e) {
    e.stopPropagation();
    this.browserProxy_.removeItem(this.model.site);
  },
});
