// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as state from './state.js';
import {Mode,
        Resolution,  // eslint-disable-line no-unused-vars
} from './type.js';

/**
 * Event builder for basic metrics.
 * @type {?analytics.EventBuilder}
 */
let base = null;

/* global analytics */

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
 */
const ga = (function() {
  const id = 'UA-134822711-1';
  const service = analytics.getService('chrome-camera-app');

  const getConfig = () =>
      new Promise((resolve) => service.getConfig().addCallback(resolve));
  const checkEnabled = () => {
    return new Promise((resolve) => {
      try {
        chrome.metricsPrivate.getIsCrashReportingEnabled(resolve);
      } catch (e) {
        resolve(false);  // Disable reporting by default.
      }
    });
  };
  const initBuilder = () => {
    return new Promise((resolve) => {
             try {
               chrome.chromeosInfoPrivate.get(
                   ['board'], (values) => resolve(values['board']));
             } catch (e) {
               resolve('');
             }
           })
        .then((board) => {
          const boardName = /^(x86-)?(\w*)/.exec(board)[0];
          const match = navigator.appVersion.match(/CrOS\s+\S+\s+([\d.]+)/);
          const osVer = match ? match[1] : '';
          base = analytics.EventBuilder.builder()
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
 */
function launchType(ackMigrate) {
  return base.category('launch').action('start').label(
      ackMigrate ? 'ack-migrate' : '');
}

/**
 * Types of intent result dimension.
 * @enum {string}
 */
export const IntentResultType = {
  NOT_INTENT: '',
  CANCELED: 'canceled',
  CONFIRMED: 'confirmed',
};

/**
 * Returns event builder for the metrics type: capture.
 * @param {?string} facingMode Camera facing-mode of the capture.
 * @param {number} length Length of 1 minute buckets for captured video.
 * @param {!Resolution} resolution Capture resolution.
 * @param {!IntentResultType} intentResult
 * @return {!analytics.EventBuilder}
 */
function captureType(facingMode, length, resolution, intentResult) {
  /**
   * @param {!Array<state.StateUnion>} states
   * @param {state.StateUnion=} cond
   * @param {boolean=} strict
   * @return {string}
   */
  const condState = (states, cond = undefined, strict = undefined) => {
    // Return the first existing state among the given states only if there is
    // no gate condition or the condition is met.
    const prerequisite = !cond || state.get(cond);
    if (strict && !prerequisite) {
      return '';
    }
    return prerequisite && states.find((s) => state.get(s)) || 'n/a';
  };

  const State = state.State;
  return base.category('capture')
      .action(condState(Object.values(Mode)))
      .label(facingMode || '(not set)')
      // Skips 3rd dimension for obsolete 'sound' state.
      .dimen(4, condState([State.MIRROR]))
      .dimen(
          5,
          condState(
              [State.GRID_3x3, State.GRID_4x4, State.GRID_GOLDEN], State.GRID))
      .dimen(6, condState([State.TIMER_3SEC, State.TIMER_10SEC], State.TIMER))
      .dimen(7, condState([State.MIC], Mode.VIDEO, true))
      .dimen(8, condState([State.MAX_WND]))
      .dimen(9, condState([State.TALL]))
      .dimen(10, resolution.toString())
      .dimen(11, condState([State.FPS_30, State.FPS_60], Mode.VIDEO, true))
      .dimen(12, intentResult)
      .value(length || 0);
}

/**
 * Returns event builder for the metrics type: perf.
 * @param {string} event The target event type.
 * @param {number} duration The duration of the event in ms.
 * @param {Object=} extras Optional information for the event.
 * @return {!analytics.EventBuilder}
 */
function perfType(event, duration, extras = {}) {
  const {resolution = ''} = extras;
  return base.category('perf')
      .action(event)
      // Round the duration here since GA expects that the value is an integer.
      // Reference: https://support.google.com/analytics/answer/1033068
      .value(Math.round(duration))
      .dimen(3, `${resolution}`);
}

/**
 * Metrics types.
 * @enum {function(...): !analytics.EventBuilder}
 */
export const Type = {
  LAUNCH: launchType,
  CAPTURE: captureType,
  PERF: perfType,
};

/**
 * Logs the given metrics.
 * @param {!Type} type Metrics type.
 * @param {...*} args Optional rest parameters for logging metrics.
 */
export function log(type, ...args) {
  ga.then((tracker) => tracker.send(type(...args)));
}
