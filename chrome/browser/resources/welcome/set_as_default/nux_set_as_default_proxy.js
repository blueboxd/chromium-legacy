// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('welcome', function() {
  const NUX_SET_AS_DEFAULT_INTERACTION_METRIC_NAME =
      'FirstRun.NewUserExperience.SetAsDefaultInteraction';

  /**
   * NuxSetAsDefaultInteractions enum.
   * These values are persisted to logs and should not be renumbered or re-used.
   * See tools/metrics/histograms/enums.xml.
   * @enum {number}
   */
  const NuxSetAsDefaultInteractions = {
    PageShown: 0,
    NavigatedAway: 1,
    Skip: 2,
    ClickSetDefault: 3,
    SuccessfullySetDefault: 4,
    NavigatedAwayThroughBrowserHistory: 5,
  };

  const NUX_SET_AS_DEFAULT_INTERACTIONS_COUNT =
      Object.keys(NuxSetAsDefaultInteractions).length;

  /** @interface */
  class NuxSetAsDefaultProxy {
    /** @return {!Promise<!welcome.DefaultBrowserInfo>} */
    requestDefaultBrowserState() {}
    setAsDefault() {}
    recordPageShown() {}
    recordNavigatedAway() {}
    recordNavigatedAwayThroughBrowserHistory() {}
    recordSkip() {}
    recordBeginSetDefault() {}
    recordSuccessfullySetDefault() {}
  }

  /** @implements {welcome.NuxSetAsDefaultProxy} */
  class NuxSetAsDefaultProxyImpl {
    /** @override */
    requestDefaultBrowserState() {
      return cr.sendWithPromise('requestDefaultBrowserState');
    }

    /** @override */
    setAsDefault() {
      chrome.send('setAsDefaultBrowser');
    }

    /** @override */
    recordPageShown() {
      this.recordInteraction_(NuxSetAsDefaultInteractions.PageShown);
    }

    /** @override */
    recordNavigatedAway() {
      this.recordInteraction_(NuxSetAsDefaultInteractions.NavigatedAway);
    }

    /** @override */
    recordNavigatedAwayThroughBrowserHistory() {
      this.recordInteraction_(
          NuxSetAsDefaultInteractions.NavigatedAwayThroughBrowserHistory);
    }

    /** @override */
    recordSkip() {
      this.recordInteraction_(NuxSetAsDefaultInteractions.Skip);
    }

    /** @override */
    recordBeginSetDefault() {
      this.recordInteraction_(NuxSetAsDefaultInteractions.ClickSetDefault);
    }

    /** @override */
    recordSuccessfullySetDefault() {
      this.recordInteraction_(
          NuxSetAsDefaultInteractions.SuccessfullySetDefault);
    }

    /**
     * @param {number} interaction
     * @private
     */
    recordInteraction_(interaction) {
      chrome.metricsPrivate.recordEnumerationValue(
          NUX_SET_AS_DEFAULT_INTERACTION_METRIC_NAME, interaction,
          NUX_SET_AS_DEFAULT_INTERACTIONS_COUNT);
    }
  }

  cr.addSingletonGetter(NuxSetAsDefaultProxyImpl);

  return {
    NuxSetAsDefaultProxy: NuxSetAsDefaultProxy,
    NuxSetAsDefaultProxyImpl: NuxSetAsDefaultProxyImpl,
  };
});
