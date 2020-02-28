// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-search-page' is the settings page containing search settings.
 */
Polymer({
  is: 'settings-search-page',

  properties: {
    prefs: Object,

    /**
     * List of default search engines available.
     * @private {!Array<!SearchEngine>}
     */
    searchEngines_: {
      type: Array,
      value() {
        return [];
      }
    },

    /** @private Filter applied to search engines. */
    searchEnginesFilter_: String,

    /** @type {?Map<string, string>} */
    focusConfig_: Object,
  },

  /** @private {?settings.SearchEnginesBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = settings.SearchEnginesBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    // Omnibox search engine
    const updateSearchEngines = searchEngines => {
      this.set('searchEngines_', searchEngines.defaults);
    };
    this.browserProxy_.getSearchEnginesList().then(updateSearchEngines);
    cr.addWebUIListener('search-engines-changed', updateSearchEngines);

    this.focusConfig_ = new Map();
    if (settings.routes.SEARCH_ENGINES) {
      this.focusConfig_.set(
          settings.routes.SEARCH_ENGINES.path, '#enginesSubpageTrigger');
    }
  },

  /** @private */
  onChange_() {
    const select = /** @type {!HTMLSelectElement} */ (this.$$('select'));
    const searchEngine = this.searchEngines_[select.selectedIndex];
    this.browserProxy_.setDefaultSearchEngine(searchEngine.modelIndex);
  },

  /** @private */
  onDisableExtension_() {
    this.fire('refresh-pref', 'default_search_provider.enabled');
  },

  /** @private */
  onManageSearchEnginesTap_() {
    settings.Router.getInstance().navigateTo(settings.routes.SEARCH_ENGINES);
  },

  /**
   * @param {!chrome.settingsPrivate.PrefObject} pref
   * @return {boolean}
   * @private
   */
  isDefaultSearchControlledByPolicy_(pref) {
    return pref.controlledBy == chrome.settingsPrivate.ControlledBy.USER_POLICY;
  },

  /**
   * @param {!chrome.settingsPrivate.PrefObject} pref
   * @return {boolean}
   * @private
   */
  isDefaultSearchEngineEnforced_(pref) {
    return pref.enforcement == chrome.settingsPrivate.Enforcement.ENFORCED;
  },
});
