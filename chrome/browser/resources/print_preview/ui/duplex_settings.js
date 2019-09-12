// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-duplex-settings',

  behaviors: [SettingsBehavior, print_preview.SelectBehavior],

  properties: {
    dark: Boolean,

    disabled: Boolean,

    /**
     * Mirroring the enum so that it can be used from HTML bindings.
     * @private
     */
    duplexValueEnum_: {
      type: Object,
      value: print_preview.DuplexMode,
    },
  },

  observers: [
    'onDuplexSettingChange_(settings.duplex.*)',
    'onDuplexTypeChange_(settings.duplexShortEdge.*)',
  ],

  /** @private {!IronMetaElement} */
  meta_: /** @type {!IronMetaElement} */ (
      Polymer.Base.create('iron-meta', {type: 'iconset'})),

  /** @private */
  onDuplexSettingChange_: function() {
    this.$.duplex.checked = this.getSettingValue('duplex');
  },

  /** @private */
  onDuplexTypeChange_: function() {
    this.selectedValue = this.getSettingValue('duplexShortEdge') ?
        this.duplexValueEnum_.SHORT_EDGE.toString() :
        this.duplexValueEnum_.LONG_EDGE.toString();
  },

  /** @private */
  onCheckboxChange_: function() {
    this.setSetting('duplex', this.$.duplex.checked);
  },

  onProcessSelectChange: function(value) {
    this.setSetting(
        'duplexShortEdge',
        value === this.duplexValueEnum_.SHORT_EDGE.toString());
  },

  /**
   * @return {boolean} Whether to expand the collapse for the dropdown.
   * @private
   */
  getOpenCollapse_: function() {
    return this.getSetting('duplexShortEdge').available &&
        /** @type {boolean} */ (this.getSettingValue('duplex'));
  },

  /**
   * @param {boolean} managed Whether the setting is managed by policy.
   * @param {boolean} disabled value of this.disabled
   * @return {boolean} Whether the controls should be disabled.
   * @private
   */
  getDisabled_: function(managed, disabled) {
    return managed || disabled;
  },

  /**
   * @return {string} An inline svg corresponding to |icon| and the image for
   *     the dropdown arrow.
   * @private
   */
  getBackgroundImages_: function() {
    const icon =
        this.getSettingValue('duplexShortEdge') ? 'short-edge' : 'long-edge';
    const iconset = /** @type {!IronIconsetSvgElement} */ (
        this.meta_.byKey('print-preview'));
    return getSelectDropdownBackground(iconset, icon, this);
  },
});
