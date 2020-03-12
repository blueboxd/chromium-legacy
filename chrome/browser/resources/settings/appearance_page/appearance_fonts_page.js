// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

/** @type {!Array<number>} */
const FONT_SIZE_RANGE = [
  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 24,
  26, 28, 30, 32, 34, 36, 40, 44, 48, 56, 64, 72,
];

/** @type {!Array<number>} */
const MINIMUM_FONT_SIZE_RANGE =
    [0, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 24];

/**
 * @param {!Array<number>} ticks
 * @return {!Array<!cr_slider.SliderTick>}
 */
function ticksWithLabels(ticks) {
  return ticks.map(x => ({label: `${x}`, value: x}));
}

/**
 * 'settings-appearance-fonts-page' is the settings page containing appearance
 * settings.
 */
Polymer({
  is: 'settings-appearance-fonts-page',

  behaviors: [I18nBehavior, WebUIListenerBehavior],

  properties: {
    /** @private {!DropdownMenuOptionList} */
    fontOptions_: Object,

    /**
     * Common font sizes.
     * @private {!Array<!cr_slider.SliderTick>}
     */
    fontSizeRange_: {
      readOnly: true,
      type: Array,
      value: ticksWithLabels(FONT_SIZE_RANGE),
    },

    /**
     * Reasonable, minimum font sizes.
     * @private {!Array<!cr_slider.SliderTick>}
     */
    minimumFontSizeRange_: {
      readOnly: true,
      type: Array,
      value: ticksWithLabels(MINIMUM_FONT_SIZE_RANGE),
    },

    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },
  },

  observers: [
    'onMinimumSizeChange_(prefs.webkit.webprefs.minimum_font_size.value)',
  ],

  /** @private {?settings.FontsBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = settings.FontsBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    this.browserProxy_.fetchFontsData().then(this.setFontsData_.bind(this));
  },

  /**
   * @param {!FontsData} response A list of fonts.
   * @private
   */
  setFontsData_(response) {
    const fontMenuOptions = [];
    for (const fontData of response.fontList) {
      fontMenuOptions.push({value: fontData[0], name: fontData[1]});
    }
    this.fontOptions_ = fontMenuOptions;
  },

  /**
   * Get the minimum font size, accounting for unset prefs.
   * @return {number}
   * @private
   */
  computeMinimumFontSize_() {
    const prefValue = this.get('prefs.webkit.webprefs.minimum_font_size.value');
    return /** @type {number} */ (prefValue) || MINIMUM_FONT_SIZE_RANGE[0];
  },


  /** @private */
  onMinimumSizeChange_() {
    this.$.minimumSizeSample.hidden = this.computeMinimumFontSize_() <= 0;
  },
});
})();
