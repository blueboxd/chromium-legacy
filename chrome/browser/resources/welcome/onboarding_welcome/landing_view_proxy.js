// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('welcome', function() {
  const NUX_LANDING_PAGE_INTERACTION_METRIC_NAME =
      'FirstRun.NewUserExperience.LandingPageInteraction';

  /**
   * NuxLandingPageInteractions enum.
   * These values are persisted to logs and should not be renumbered or re-used.
   * See tools/metrics/histograms/enums.xml.
   * @enum {number}
   */
  const NuxLandingPageInteractions = {
    PageShown: 0,
    NavigatedAway: 1,
    NewUser: 2,
    ExistingUser: 3,
  };

  const NUX_LANDING_PAGE_INTERACTIONS_COUNT =
      Object.keys(NuxLandingPageInteractions).length;

  /** @interface */
  class LandingViewProxy {
    recordPageShown() {}

    recordNavigatedAway() {}

    recordNewUser() {}

    recordExistingUser() {}
  }

  /** @implements {welcome.LandingViewProxy} */
  class LandingViewProxyImpl {
    /** @override */
    recordPageShown() {
      this.recordInteraction_(NuxLandingPageInteractions.PageShown);
    }

    /** @override */
    recordNavigatedAway() {
      this.recordInteraction_(NuxLandingPageInteractions.NavigatedAway);
    }

    /** @override */
    recordNewUser() {
      this.recordInteraction_(NuxLandingPageInteractions.NewUser);
    }

    /** @override */
    recordExistingUser() {
      this.recordInteraction_(NuxLandingPageInteractions.ExistingUser);
    }

    /**
     * @param {number} interaction
     * @private
     */
    recordInteraction_(interaction) {
      chrome.metricsPrivate.recordEnumerationValue(
          NUX_LANDING_PAGE_INTERACTION_METRIC_NAME, interaction,
          NUX_LANDING_PAGE_INTERACTIONS_COUNT);
    }
  }

  cr.addSingletonGetter(LandingViewProxyImpl);

  return {
    LandingViewProxy: LandingViewProxy,
    LandingViewProxyImpl: LandingViewProxyImpl,
  };
});
