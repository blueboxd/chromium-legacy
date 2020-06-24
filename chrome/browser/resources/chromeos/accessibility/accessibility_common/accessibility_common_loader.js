// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to manage loading resources depending on which Accessibility features
 * are enabled.
 */
class AccessibilityCommon {
  constructor() {
    /**
     * @private {Autoclick}
     */
    this.autoclick_ = null;

    this.init_();
  }

  /**
   * @public {Autoclick}
   */
  getAutoclickForTest() {
    return this.autoclick_;
  }

  /**
   * Initializes the AccessibilityCommon extension.
   * @private
   */
  init_() {
    chrome.accessibilityFeatures.autoclick.get(
        {}, this.onAutoclickUpdated_.bind(this));
    chrome.accessibilityFeatures.autoclick.onChange.addListener(
        this.onAutoclickUpdated_.bind(this));
  }

  /**
   * Called when the autoclick feature is enabled or disabled.
   * @param {*} details
   * @private
   */
  onAutoclickUpdated_(details) {
    if (details.value && !this.autoclick_) {
      // Initialize the Autoclick extension.
      this.autoclick_ = new Autoclick();
    } else if (!details.value && this.autoclick_) {
      // TODO(crbug.com/1096759): Consider using XHR to load/unload autoclick
      // rather than relying on a destructor to clean up state.
      this.autoclick_.onAutoclickDisabled();
      this.autoclick_ = null;
    }
  }
}

// Initialize the AccessibilityCommon extension.
var accessibilityCommon = new AccessibilityCommon();
