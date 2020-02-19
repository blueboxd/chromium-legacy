// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

/**
 * Enum to represent each page in the gesture navigation screen.
 * @enum {string}
 */
const gesturePage = {
  INTRO: 'gestureIntro',
  HOME: 'gestureHome',
  BACK: 'gestureBack',
  OVERVIEW: 'gestureOverview'
};

Polymer({
  is: 'gesture-navigation',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  properties: {
    /** @private */
    currentPage_: {type: String, value: gesturePage.INTRO},
  },

  /** @override */
  ready() {
    this.initializeLoginScreen('GestureNavigationScreen', {
      commonScreenSize: true,
      resetAllowed: true,
    });
  },

  /**
   * Called before the screen is shown.
   */
  onBeforeShow() {
    this.currentPage_ = gesturePage.INTRO;
    this.behaviors.forEach((behavior) => {
      if (behavior.onBeforeShow)
        behavior.onBeforeShow.call(this);
    });
  },

  /**
   * This is 'on-tap' event handler for 'next' or 'get started' button.
   * @private
   *
   */
  onNext_() {
    switch (this.currentPage_) {
      case gesturePage.INTRO:
        this.currentPage_ = gesturePage.HOME;
        break;
      case gesturePage.HOME:
        this.currentPage_ = gesturePage.BACK;
        break;
      case gesturePage.BACK:
        this.currentPage_ = gesturePage.OVERVIEW;
        break;
      case gesturePage.OVERVIEW:
        this.userActed('exit');
        break;
    }
  },

  /**
   * Comparison function for the current page.
   * @private
   */
  isEqual_(currentPage_, page_) {
    return currentPage_ == page_;
  },
});
})();