// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview settings-startup-url-entry represents a UI component that
 * displays a URL that is loaded during startup. It includes a menu that allows
 * the user to edit/remove the entry.
 */

cr.define('settings', function() {
  /**
   * The name of the event fired from this element when the "Edit" option is
   * clicked.
   * @type {string}
   */
  const EDIT_STARTUP_URL_EVENT = 'edit-startup-url';

  return {EDIT_STARTUP_URL_EVENT};
});

Polymer({
  is: 'settings-startup-url-entry',

  behaviors: [cr.ui.FocusRowBehavior],

  properties: {
    editable: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /** @type {!StartupPageInfo} */
    model: Object,
  },

  /** @private */
  onRemoveTap_() {
    this.$$('cr-action-menu').close();
    settings.StartupUrlsPageBrowserProxyImpl.getInstance().removeStartupPage(
        this.model.modelIndex);
  },

  /**
   * @param {!Event} e
   * @private
   */
  onEditTap_(e) {
    e.preventDefault();
    this.$$('cr-action-menu').close();
    this.fire(settings.EDIT_STARTUP_URL_EVENT, {
      model: this.model,
      anchor: this.$$('#dots'),
    });
  },

  /** @private */
  onDotsTap_() {
    const actionMenu =
        /** @type {!CrActionMenuElement} */ (this.$$('#menu').get());
    actionMenu.showAt(assert(this.$$('#dots')));
  },
});
