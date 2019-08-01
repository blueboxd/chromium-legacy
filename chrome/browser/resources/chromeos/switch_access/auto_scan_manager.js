// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to handle auto-scan behavior.
 */
class AutoScanManager {
  /**
   * @param {!SwitchAccessInterface} switchAccess
   */
  constructor(switchAccess) {
    /**
     * SwitchAccess reference.
     * @private {!SwitchAccessInterface}
     */
    this.switchAccess_ = switchAccess;

    /**
     * Auto-scan interval ID.
     * @private {number|undefined}
     */
    this.intervalID_;

    /**
     * Length of auto-scan interval in milliseconds.
     * @private {number}
     */
    this.scanTime_ = SCAN_TIME_NOT_INITIALIZED;
  }

  /** Finishes setup of auto scan manager once Prefs are loaded. */
  onPrefsReady() {
    this.scanTime_ = this.switchAccess_.getNumberPreference(
        SAConstants.Preference.AUTO_SCAN_TIME);
    const enabled = this.switchAccess_.getBooleanPreference(
        SAConstants.Preference.AUTO_SCAN_ENABLED);
    if (enabled) this.start_();
  }

  /**
   * Return true if auto-scan is currently running. Otherwise return false.
   * @return {boolean}
   */
  isRunning() {
    return this.intervalID_ !== undefined;
  }

  /**
   * Restart auto-scan under the current settings if it is currently running.
   */
  restartIfRunning() {
    if (this.isRunning()) {
      this.stop_();
      this.start_();
    }
  }

  /**
   * Stop auto-scan if it is currently running. Then, if |enabled| is true,
   * turn on auto-scan. Otherwise leave it off.
   *
   * @param {boolean} enabled
   */
  setEnabled(enabled) {
    if (this.isRunning())
      this.stop_();
    if (enabled)
      this.start_();
  }

  /**
   * Update this.scanTime_ to |scanTime|. Then, if auto-scan is currently
   * running, restart it.
   *
   * @param {number} scanTime Auto-scan interval time in milliseconds.
   */
  setScanTime(scanTime) {
    this.scanTime_ = scanTime;
    this.restartIfRunning();
  }

  /**
   * Stop the window from moving to the next node at a fixed interval.
   * @private
   */
  stop_() {
    window.clearInterval(this.intervalID_);
    this.intervalID_ = undefined;
  }

  /**
   * Set the window to move to the next node at an interval in milliseconds
   * equal to this.scanTime_.
   * @private
   */
  start_() {
    if (this.scanTime_ === SCAN_TIME_NOT_INITIALIZED)
      return;

    this.intervalID_ = window.setInterval(
        this.switchAccess_.moveForward.bind(this.switchAccess_),
        this.scanTime_);
  }
}

/**
 * Sentinel value that indicates an uninitialized scan time.
 * @type {number}
 */
const SCAN_TIME_NOT_INITIALIZED = -1;
