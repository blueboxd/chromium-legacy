// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview First user log in App Downloading screen implementation.
 */

login.createScreen('AppDownloadingScreen', 'app-downloading', function() {
  return {
    EXTERNAL_API: ['updateNumberOfSelectedApps'],

    /** Initial UI State for screen */
    getOobeUIInitialState() {
      return OOBE_UI_STATE.ONBOARDING;
    },

    /**
     * Returns the control which should receive initial focus.
     */
    get defaultControl() {
      return $('app-downloading-screen');
    },

    /*
     * Executed on language change.
     */
    updateLocalizedContent() {
      $('app-downloading-screen').i18nUpdateLocale();
    },

    /**
     * Set the number of apps that are selected by the user. It is used in the
     * screen title.
     * @param numOfApps Number of apps selected by the user.
     */
    updateNumberOfSelectedApps(numOfApps) {
      $('app-downloading-screen').numOfApps = numOfApps;
    },
  };
});
