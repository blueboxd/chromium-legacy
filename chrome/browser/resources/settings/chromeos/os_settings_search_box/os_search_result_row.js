// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-search-result-row' is the container for one search result.
 */

Polymer({
  is: 'os-search-result-row',

  behaviors: [cr.ui.FocusRowBehavior],

  properties: {
    // Whether the search result row is selected.
    selected: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /** @type {!chromeos.settings.mojom.SearchResult} */
    searchResult: Object,
  },

  /**
   * @return {string} Exact string of the result to be displayed.
   */
  getResultText_() {
    // The C++ layer stores the text result as an array of 16 bit char codes,
    // so it must be converted to a JS String.
    return String.fromCharCode.apply(null, this.searchResult.resultText.data);
  },

  /**
   * Only relevant when the focus-row-control is focus()ed. This keypress
   * handler specifies that pressing 'Enter' should cause a route change.
   * TODO(crbug/1056909): Add test for this specific case.
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyPress_(e) {
    if (e.key === 'Enter') {
      e.stopPropagation();
      this.navigateToSearchResultRoute();
    }
  },

  navigateToSearchResultRoute() {
    assert(this.searchResult.urlPathWithParameters, 'Url path is empty.');

    // |this.searchResult.urlPathWithParameters| separates the path and params
    // by a '?' char.
    const pathAndOptParams = this.searchResult.urlPathWithParameters.split('?');

    // There should be at most 2 items in the array (the path and the params).
    assert(pathAndOptParams.length <= 2, 'Path and params format error.');

    const route = assert(
        settings.Router.getInstance().getRouteForPath(
            '/' + pathAndOptParams[0]),
        'Supplied path does not map to an existing route.');
    const params = pathAndOptParams.length == 2 ?
        new URLSearchParams(pathAndOptParams[1]) :
        undefined;
    settings.Router.getInstance().navigateTo(route, params);
    this.fire('navigated-to-result-route');
  },

  /**
   * @return {string} The name of the icon to use.
   * @private
   */
  getResultIcon_() {
    const Icon = chromeos.settings.mojom.SearchResultIcon;
    switch (this.searchResult.icon) {
      case Icon.kCellular:
        return 'os-settings:multidevice-better-together-suite';
      case Icon.kEthernet:
        return 'os-settings:settings-ethernet';
      case Icon.kWifi:
        return 'os-settings:network-wifi';
      default:
        // TODO(crbug/1056909): assertNotReached() when all icons are added.
        return 'os-settings:settings-general';
    }
  },
});
