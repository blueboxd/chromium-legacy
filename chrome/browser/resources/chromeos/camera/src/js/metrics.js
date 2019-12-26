// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for metrics.
 */
cca.metrics = cca.metrics || {};

/**
 * Event builder for basic metrics.
 * @type {?analytics.EventBuilder}
 * @private
 */
cca.metrics.base_ = null;

var analytics = window['analytics'] || {};

/**
 * Fixes analytics.EventBuilder's dimension() method.
 * @param {number} i
 * @param {string} v
 * @return {!analytics.EventBuilder}
 */
analytics.EventBuilder.prototype.dimen = function(i, v) {
  return this.dimension({index: i, value: v});
};

/**
 * Promise for Google Analytics tracker.
 * @type {Promise<analytics.Tracker>}
 * @suppress {checkTypes}
 * @private
 */
cca.metrics.ga_ = (function() {
  const id = 'UA-134822711-1';
  const service = analytics.getService('chrome-camera-app');

  var getConfig = () =>
      new Promise((resolve) => service.getConfig().addCallback(resolve));
  var checkEnabled = () => {
    return new Promise((resolve) => {
      try {
        chrome.metricsPrivate.getIsCrashReportingEnabled(resolve);
      } catch (e) {
        resolve(false);  // Disable reporting by default.
      }
    });
  };
  var initBuilder = () => {
    return new Promise((resolve) => {
             try {
               chrome.chromeosInfoPrivate.get(
                   ['board'], (values) => resolve(values['board']));
             } catch (e) {
               resolve('');
             }
           })
        .then((board) => {
          var boardName = /^(x86-)?(\w*)/.exec(board)[0];
          var match = navigator.appVersion.match(/CrOS\s+\S+\s+([\d.]+)/);
          var osVer = match ? match[1] : '';
          cca.metrics.base_ = analytics.EventBuilder.builder()
                                  .dimen(1, boardName)
                                  .dimen(2, osVer);
        });
  };

  return Promise.all([getConfig(), checkEnabled(), initBuilder()])
      .then(([config, enabled]) => {
        config.setTrackingPermitted(enabled);
        return service.getTracker(id);
      });
})();

/**
 * Returns event builder for the metrics type: launch.
 * @param {boolean} ackMigrate Whether acknowledged to migrate during launch.
 * @return {!analytics.EventBuilder}
 * @private
 */
cca.metrics.launchType_ = function(ackMigrate) {
  return cca.metrics.base_.category('launch').action('start').label(
      ackMigrate ? 'ack-migrate' : '');
};

/**
 * Types of intent result dimension.
 * @enum {string}
 */
cca.metrics.IntentResultType = {
  NOT_INTENT: '',
  CANCELED: 'canceled',
  CONFIRMED: 'confirmed',
};

/**
 * Returns event builder for the metrics type: capture.
 * @param {?string} facingMode Camera facing-mode of the capture.
 * @param {number} length Length of 1 minute buckets for captured video.
 * @param {!cca.Resolution} resolution Capture resolution.
 * @param {!cca.metrics.IntentResultType} intentResult
 * @return {!analytics.EventBuilder}
 * @private
 */
cca.metrics.captureType_ = function(
    facingMode, length, resolution, intentResult) {
  /**
   * @param {!Array<cca.state.StateUnion>} states
   * @param {cca.state.StateUnion=} cond
   * @param {boolean=} strict
   * @return {string}
   */
  const condState = (states, cond = undefined, strict = undefined) => {
    // Return the first existing state among the given states only if there is
    // no gate condition or the condition is met.
    const prerequisite = !cond || cca.state.get(cond);
    if (strict && !prerequisite) {
      return '';
    }
    return prerequisite && states.find((state) => cca.state.get(state)) ||
        'n/a';
  };

  const State = cca.state.State;
  return cca.metrics.base_.category('capture')
      .action(condState(Object.values(cca.Mode)))
      .label(facingMode || '(not set)')
      // Skips 3rd dimension for obsolete 'sound' state.
      .dimen(4, condState([State.MIRROR]))
      .dimen(
          5,
          condState(
              [State.GRID_3x3, State.GRID_4x4, State.GRID_GOLDEN], State.GRID))
      .dimen(6, condState([State.TIMER_3SEC, State.TIMER_10SEC], State.TIMER))
      .dimen(7, condState([State.MIC], cca.Mode.VIDEO, true))
      .dimen(8, condState([State.MAX_WND]))
      .dimen(9, condState([State.TALL]))
      .dimen(10, resolution.toString())
      .dimen(11, condState([State.FPS_30, State.FPS_60], cca.Mode.VIDEO, true))
      .dimen(12, intentResult)
      .value(length || 0);
};

/**
 * Returns event builder for the metrics type: perf.
 * @param {string} event The target event type.
 * @param {number} duration The duration of the event in ms.
 * @param {Object=} extras Optional information for the event.
 * @return {!analytics.EventBuilder}
 * @private
 */
cca.metrics.perfType_ = function(event, duration, extras = {}) {
  const {resolution = ''} = extras;
  return cca.metrics.base_.category('perf')
      .action(event)
      // Round the duration here since GA expects that the value is an integer.
      // Reference: https://support.google.com/analytics/answer/1033068
      .value(Math.round(duration))
      .dimen(3, `${resolution}`);
};

/**
 * Metrics types.
 * @enum {function(...): !analytics.EventBuilder}
 */
cca.metrics.Type = {
  LAUNCH: cca.metrics.launchType_,
  CAPTURE: cca.metrics.captureType_,
  PERF: cca.metrics.perfType_,
};

/**
 * Logs the given metrics.
 * @param {!cca.metrics.Type} type Metrics type.
 * @param {...*} args Optional rest parameters for logging metrics.
 */
cca.metrics.log = function(type, ...args) {
  cca.metrics.ga_.then((tracker) => tracker.send(type(...args)));
};
