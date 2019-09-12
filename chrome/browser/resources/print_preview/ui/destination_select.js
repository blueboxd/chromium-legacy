// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-destination-select',

  behaviors: [I18nBehavior, print_preview.SelectBehavior],

  properties: {
    activeUser: String,

    appKioskMode: Boolean,

    dark: Boolean,

    /** @type {!print_preview.Destination} */
    destination: Object,

    disabled: Boolean,

    noDestinations: Boolean,

    /** @type {!Array<!print_preview.RecentDestination>} */
    recentDestinationList: Array,
  },

  /** @private {!IronMetaElement} */
  meta_: /** @type {!IronMetaElement} */ (
      Polymer.Base.create('iron-meta', {type: 'iconset'})),

  focus: function() {
    this.$$('.md-select').focus();
  },

  /** Sets the select to the current value of |destination|. */
  updateDestination: function() {
    this.selectedValue = this.destination.key;
  },

  /**
   * @return {string} Unique identifier for the Save as PDF destination
   * @private
   */
  getPdfDestinationKey_: function() {
    return print_preview.createDestinationKey(
        print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
        print_preview.DestinationOrigin.LOCAL, '');
  },

  /**
   * @return {string} Unique identifier for the Save to Google Drive destination
   * @private
   */
  getGoogleDriveDestinationKey_: function() {
    return print_preview.createDestinationKey(
        print_preview.Destination.GooglePromotedId.DOCS,
        print_preview.DestinationOrigin.COOKIES, this.activeUser);
  },

  /**
   * @param {string} icon The icon set and icon to obtain.
   * @return {string} An inline svg corresponding to |icon| and the image for
   *     the dropdown arrow.
   * @private
   */
  getBackgroundImages_: function(icon) {
    if (!icon) {
      return '';
    }

    let iconSetAndIcon = null;
    // <if expr="chromeos">
    if (this.noDestinations) {
      iconSetAndIcon = ['cr', 'error'];
    }
    // </if>
    iconSetAndIcon = iconSetAndIcon || icon.split(':');
    const iconset = /** @type {!IronIconsetSvgElement} */ (
        this.meta_.byKey(iconSetAndIcon[0]));
    return getSelectDropdownBackground(iconset, iconSetAndIcon[1], this);
  },

  /** @private */
  onProcessSelectChange: function(value) {
    this.fire('selected-option-change', value);
  },

  /**
   * @param {!print_preview.RecentDestination} recentDestination
   * @return {string} Key for the recent destination
   * @private
   */
  getKey_: function(recentDestination) {
    return print_preview.createRecentDestinationKey(recentDestination);
  },
});
