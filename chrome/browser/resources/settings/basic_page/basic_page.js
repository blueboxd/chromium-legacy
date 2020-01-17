// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-basic-page' is the settings page containing the actual settings.
 */
(function() {
'use strict';

// <if expr="chromeos">
const OS_BANNER_INTERACTION_METRIC_NAME =
    'ChromeOS.Settings.OsBannerInteraction';

/**
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
const CrosSettingsOsBannerInteraction = {
  NotShown: 0,
  Shown: 1,
  Clicked: 2,
  Closed: 3,
};
// </if>

Polymer({
  is: 'settings-basic-page',

  behaviors: [
    settings.MainPageBehavior,
    settings.RouteObserverBehavior,
    PrefsBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    showChangePassword: {
      type: Boolean,
      value: false,
    },

    /**
     * Dictionary defining page visibility.
     * @type {!PageVisibility}
     */
    pageVisibility: {
      type: Object,
      value() {
        return {};
      },
    },

    advancedToggleExpanded: {
      type: Boolean,
      value: false,
      notify: true,
      observer: 'advancedToggleExpandedChanged_',
    },

    /**
     * True if a section is fully expanded to hide other sections beneath it.
     * False otherwise (even while animating a section open/closed).
     * @private {boolean}
     */
    hasExpandedSection_: {
      type: Boolean,
      value: false,
    },

    /**
     * True if the basic page should currently display the reset profile banner.
     * @private {boolean}
     */
    showResetProfileBanner_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('showResetProfileBanner');
      },
    },

    // <if expr="chromeos">
    /**
     * Whether the user is a secondary user. Computed so that it is calculated
     * correctly after loadTimeData is available.
     * @private
     */
    showSecondaryUserBanner_: {
      type: Boolean,
      computed: 'computeShowSecondaryUserBanner_(hasExpandedSection_)',
    },

    /** @private */
    showOSSettingsBanner_: {
      type: Boolean,
      computed: 'computeShowOSSettingsBanner_(' +
          'prefs.settings.cros.show_os_banner.value, currentRoute_)',
    },
    // </if>

    /** @private {!settings.Route|undefined} */
    currentRoute_: Object,
  },

  hostAttributes: {
    role: 'main',
  },

  listeners: {
    'subpage-expand': 'onSubpageExpanded_',
  },

  /**
   * Used to avoid handling a new toggle while currently toggling.
   * @private {boolean}
   */
  advancedTogglingInProgress_: false,

  // <if expr="chromeos">
  /** @private {boolean} */
  osBannerShowMetricRecorded_: false,
  // </if>

  /** @override */
  attached() {
    this.currentRoute_ = settings.Router.getInstance().getCurrentRoute();

    this.addWebUIListener('change-password-visibility', visibility => {
      this.showChangePassword = visibility;
    });
  },

  /**
   * @param {!settings.Route} newRoute
   * @param {settings.Route} oldRoute
   */
  currentRouteChanged(newRoute, oldRoute) {
    this.currentRoute_ = newRoute;

    if (settings.routes.ADVANCED &&
        settings.routes.ADVANCED.contains(newRoute)) {
      this.advancedToggleExpanded = true;
    }

    if (oldRoute && oldRoute.isSubpage()) {
      // If the new route isn't the same expanded section, reset
      // hasExpandedSection_ for the next transition.
      if (!newRoute.isSubpage() || newRoute.section != oldRoute.section) {
        this.hasExpandedSection_ = false;
      }
    } else {
      assert(!this.hasExpandedSection_);
    }

    settings.MainPageBehavior.currentRouteChanged.call(
        this, newRoute, oldRoute);
  },

  // Override settings.MainPageBehavior method.
  containsRoute(route) {
    return !route || settings.routes.BASIC.contains(route) ||
        settings.routes.ADVANCED.contains(route);
  },

  /**
   * @param {boolean|undefined} visibility
   * @return {boolean}
   * @private
   */
  showPage_(visibility) {
    return visibility !== false;
  },

  /**
   * Queues a task to search the basic sections, then another for the advanced
   * sections.
   * @param {string} query The text to search for.
   * @return {!Promise<!settings.SearchResult>} A signal indicating that
   *     searching finished.
   */
  searchContents(query) {
    const whenSearchDone = [
      settings.getSearchManager().search(query, assert(this.$$('#basicPage'))),
    ];

    if (this.pageVisibility.advancedSettings !== false) {
      whenSearchDone.push(
          this.$$('#advancedPageTemplate').get().then(function(advancedPage) {
            return settings.getSearchManager().search(query, advancedPage);
          }));
    }

    return Promise.all(whenSearchDone).then(function(requests) {
      // Combine the SearchRequests results to a single SearchResult object.
      return {
        canceled: requests.some(function(r) {
          return r.canceled;
        }),
        didFindMatches: requests.some(function(r) {
          return r.didFindMatches();
        }),
        // All requests correspond to the same user query, so only need to check
        // one of them.
        wasClearSearch: requests[0].isSame(''),
      };
    });
  },

  // <if expr="chromeos">
  /**
   * @return {boolean}
   * @private
   */
  computeShowSecondaryUserBanner_() {
    return !this.hasExpandedSection_ &&
        loadTimeData.getBoolean('isSecondaryUser');
  },

  /**
   * @return {boolean|undefined}
   * @private
   */
  computeShowOSSettingsBanner_() {
    // this.prefs is implicitly used by this.getPref() below.
    if (!this.prefs || !this.currentRoute_) {
      return;
    }
    const showPref = /** @type {boolean} */ (
        this.getPref('settings.cros.show_os_banner').value);

    // Banner only shows on the main page because direct navigations to a
    // sub-page are unlikely to be due to a user looking for an OS setting.
    const show = showPref && !this.currentRoute_.isSubpage();

    // Record the show metric once. We can't record the metric in attached()
    // because prefs might not be ready yet.
    if (!this.osBannerShowMetricRecorded_) {
      chrome.metricsPrivate.recordEnumerationValue(
          OS_BANNER_INTERACTION_METRIC_NAME,
          show ? CrosSettingsOsBannerInteraction.Shown :
                 CrosSettingsOsBannerInteraction.NotShown,
          Object.keys(CrosSettingsOsBannerInteraction).length);
      this.osBannerShowMetricRecorded_ = true;
    }
    return show;
  },

  /** @private */
  onOSSettingsBannerClick_() {
    // The label has a link that opens the page, so just record the metric.
    chrome.metricsPrivate.recordEnumerationValue(
        OS_BANNER_INTERACTION_METRIC_NAME,
        CrosSettingsOsBannerInteraction.Clicked,
        Object.keys(CrosSettingsOsBannerInteraction).length);
  },

  /** @private */
  onOSSettingsBannerClosed_() {
    this.setPrefValue('settings.cros.show_os_banner', false);
    chrome.metricsPrivate.recordEnumerationValue(
        OS_BANNER_INTERACTION_METRIC_NAME,
        CrosSettingsOsBannerInteraction.Closed,
        Object.keys(CrosSettingsOsBannerInteraction).length);
  },
  // </if>

  /** @private */
  onResetProfileBannerClosed_() {
    this.showResetProfileBanner_ = false;
  },

  /**
   * Hides everything but the newly expanded subpage.
   * @private
   */
  onSubpageExpanded_() {
    this.hasExpandedSection_ = true;
  },

  /**
   * Render the advanced page now (don't wait for idle).
   * @private
   */
  advancedToggleExpandedChanged_() {
    if (!this.advancedToggleExpanded) {
      return;
    }

    // In Polymer2, async() does not wait long enough for layout to complete.
    // Polymer.RenderStatus.beforeNextRender() must be used instead.
    Polymer.RenderStatus.beforeNextRender(this, () => {
      this.$$('#advancedPageTemplate').get();
    });
  },

  advancedToggleClicked_() {
    if (this.advancedTogglingInProgress_) {
      return;
    }

    this.advancedTogglingInProgress_ = true;
    const toggle = this.$$('#toggleContainer');
    if (!this.advancedToggleExpanded) {
      this.advancedToggleExpanded = true;
      this.async(() => {
        this.$$('#advancedPageTemplate').get().then(() => {
          this.fire('scroll-to-top', {
            top: toggle.offsetTop,
            callback: () => {
              this.advancedTogglingInProgress_ = false;
            }
          });
        });
      });
    } else {
      this.fire('scroll-to-bottom', {
        bottom: toggle.offsetTop + toggle.offsetHeight + 24,
        callback: () => {
          this.advancedToggleExpanded = false;
          this.advancedTogglingInProgress_ = false;
        }
      });
    }
  },

  /**
   * @param {boolean} inSearchMode
   * @param {boolean} hasExpandedSection
   * @return {boolean}
   * @private
   */
  showAdvancedToggle_(inSearchMode, hasExpandedSection) {
    return !inSearchMode && !hasExpandedSection;
  },

  /**
   * @param {!settings.Route} currentRoute
   * @param {boolean} inSearchMode
   * @param {boolean} hasExpandedSection
   * @return {boolean} Whether to show the basic page, taking into account
   *     both routing and search state.
   * @private
   */
  showBasicPage_(currentRoute, inSearchMode, hasExpandedSection) {
    return !hasExpandedSection || settings.routes.BASIC.contains(currentRoute);
  },

  /**
   * @param {!settings.Route} currentRoute
   * @param {boolean} inSearchMode
   * @param {boolean} hasExpandedSection
   * @param {boolean} advancedToggleExpanded
   * @return {boolean} Whether to show the advanced page, taking into account
   *     both routing and search state.
   * @private
   */
  showAdvancedPage_(
      currentRoute, inSearchMode, hasExpandedSection, advancedToggleExpanded) {
    return hasExpandedSection ?
        (settings.routes.ADVANCED &&
         settings.routes.ADVANCED.contains(currentRoute)) :
        advancedToggleExpanded || inSearchMode;
  },

  /**
   * @param {(boolean|undefined)} visibility
   * @return {boolean} True unless visibility is false.
   * @private
   */
  showAdvancedSettings_(visibility) {
    return visibility !== false;
  },

  /**
   * @param {boolean} opened Whether the menu is expanded.
   * @return {string} Icon name.
   * @private
   */
  getArrowIcon_(opened) {
    return opened ? 'cr:arrow-drop-up' : 'cr:arrow-drop-down';
  },

  /**
   * @param {boolean} bool
   * @return {string}
   * @private
   */
  boolToString_(bool) {
    return bool.toString();
  },
});
})();
