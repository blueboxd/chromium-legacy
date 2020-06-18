// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'setup-succeeded-page',

  properties: {
    /** Overridden from UiPageContainerBehavior. */
    forwardButtonTextId: {
      type: String,
      value: 'done',
    },
  },

  behaviors: [UiPageContainerBehavior],

  /** @private {?multidevice_setup.BrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = multidevice_setup.BrowserProxyImpl.getInstance();
  },

  /** @private */
  openSettings_() {
    this.browserProxy_.openMultiDeviceSettings();
  },

  /** @private */
  onSettingsLinkClicked_() {
    this.openSettings_();
    this.fire('setup-exited');
  },

  /** @private */
  getMessageHtml_() {
    return this.i18nAdvanced('setupSucceededPageMessage', {attrs: ['id']});
  },

  /** @override */
  ready() {
    const linkElement = this.$$('#settings-link');
    linkElement.setAttribute('href', '#');
    linkElement.addEventListener('click', () => this.onSettingsLinkClicked_());
  },
});
