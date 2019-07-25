// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('welcome', function() {
  /**
   * @typedef {{
   *   id: number,
   *   imageUrl: string,
   *   thumbnailClass: string,
   *   title: string,
   * }}
   */
  let NtpBackgroundData;

  /** @interface */
  class NtpBackgroundProxy {
    clearBackground() {}

    /** @return {!Promise<!Array<!welcome.NtpBackgroundData>>} */
    getBackgrounds() {}

    /**
     * @param {string} url
     * @return {!Promise<void>}
     */
    preloadImage(url) {}

    recordBackgroundImageFailedToLoad() {}

    /** @param {number} loadTime */
    recordBackgroundImageLoadTime(loadTime) {}

    recordBackgroundImageNeverLoaded() {}

    /** @param {number} id */
    setBackground(id) {}
  }

  /** @implements {welcome.NtpBackgroundProxy} */
  class NtpBackgroundProxyImpl {
    /** @override */
    clearBackground() {
      return cr.sendWithPromise('clearBackground');
    }

    /** @override */
    getBackgrounds() {
      return cr.sendWithPromise('getBackgrounds');
    }

    /** @override */
    preloadImage(url) {
      return new Promise((resolve, reject) => {
        const preloadedImage = new Image();
        preloadedImage.onerror = reject;
        preloadedImage.onload = resolve;
        preloadedImage.src = url;
      });
    }

    /** @override */
    recordBackgroundImageFailedToLoad() {
      const ntpInteractions =
          welcome.NtpBackgroundMetricsProxyImpl.getInstance().getInteractions();
      chrome.metricsPrivate.recordEnumerationValue(
          'FirstRun.NewUserExperience.NtpBackgroundInteraction',
          ntpInteractions.BackgroundImageFailedToLoad,
          Object.keys(ntpInteractions).length);
    }

    /** @override */
    recordBackgroundImageLoadTime(loadTime) {
      chrome.metricsPrivate.recordTime(
          'FirstRun.NewUserExperience.NtpBackgroundLoadTime', loadTime);
    }

    /** @override */
    recordBackgroundImageNeverLoaded() {
      const ntpInteractions =
          welcome.NtpBackgroundMetricsProxyImpl.getInstance().getInteractions();
      chrome.metricsPrivate.recordEnumerationValue(
          'FirstRun.NewUserExperience.NtpBackgroundInteraction',
          ntpInteractions.BackgroundImageNeverLoaded,
          Object.keys(ntpInteractions).length);
    }

    /** @override */
    setBackground(id) {
      chrome.send('setBackground', [id]);
    }
  }

  cr.addSingletonGetter(NtpBackgroundProxyImpl);

  return {
    NtpBackgroundData: NtpBackgroundData,
    NtpBackgroundProxy: NtpBackgroundProxy,
    NtpBackgroundProxyImpl: NtpBackgroundProxyImpl,
  };
});
