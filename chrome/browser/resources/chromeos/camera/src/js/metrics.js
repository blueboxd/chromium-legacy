// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxy} from './browser_proxy/browser_proxy.js';
import {assert} from './chrome_util.js';
// eslint-disable-next-line no-unused-vars
import {Intent} from './intent.js';
// eslint-disable-next-line no-unused-vars
import {PerfEvent} from './perf.js';
import * as state from './state.js';
// eslint-disable-next-line no-unused-vars
import {Facing} from './type.js';
import {
  Mode,
  Resolution,  // eslint-disable-line no-unused-vars
} from './type.js';

/**
 * @type {?Map<number, Object>}
 */
let baseDimen = null;

/**
 * @type {?Promise}
 */
let ready = null;

/**
 * Send the event to GA backend.
 * @param {!ga.Fields} event The event to send.
 * @param {Map<number, Object>=} dimen Optional object contains dimension
 *     information.
 */
function sendEvent(event, dimen = null) {
  const assignDimension = (e, d) => {
    d.forEach((value, key) => e[`dimension${key}`] = value);
  };

  assert(baseDimen !== null);
  assignDimension(event, baseDimen);
  if (dimen !== null) {
    assignDimension(event, dimen);
  }
  window.ga('send', 'event', event);
}

/**
 * Initializes metrics with parameters.
 * @param {!boolean} isTesting Whether is collecting logs for running
 *     testing.
 */
export function initMetrics(isTesting) {
  ready = (async () => {
    const GA_ID = 'UA-134822711-1';
    const canSendMetrics =
        !isTesting && await browserProxy.isCrashReportingEnabled();
    window[`ga-disable-${GA_ID}`] = !canSendMetrics;

    browserProxy.addDummyHistoryIfNotAvailable();

    // GA initialization function which is mostly copied from
    // https://developers.google.com/analytics/devguides/collection/analyticsjs.
    (function(i, s, o, g, r) {
      i['GoogleAnalyticsObject'] = r;
      i[r] = i[r] || function(...args) {
        (i[r].q = i[r].q || []).push(args);
      }, i[r].l = 1 * new Date();
      const a = s.createElement(o);
      const m = s.getElementsByTagName(o)[0];
      a.async = 1;
      a.src = g;
      m.parentNode.insertBefore(a, m);
    })(window, document, 'script', '../js/lib/analytics.js', 'ga');

    const board = await browserProxy.getBoard();
    const boardName = /^(x86-)?(\w*)/.exec(board)[0];
    const match = navigator.appVersion.match(/CrOS\s+\S+\s+([\d.]+)/);
    const osVer = match ? match[1] : '';
    baseDimen = new Map([
      [1, boardName],
      [2, osVer],
    ]);

    // By default GA stores the user ID in cookies. Change to store in local
    // storage instead.
    const GA_LOCAL_STORAGE_KEY = 'google-analytics.analytics.user-id';
    const gaLocalStorage =
        await browserProxy.localStorageGet({[GA_LOCAL_STORAGE_KEY]: null});
    window.ga('create', GA_ID, {
      'storage': 'none',
      'clientId': gaLocalStorage[GA_LOCAL_STORAGE_KEY] || null,
    });
    window.ga(
        (tracker) => browserProxy.localStorageSet(
            {[GA_LOCAL_STORAGE_KEY]: tracker.get('clientId')}));

    // By default GA uses a dummy image and sets its source to the target URL to
    // record metrics. Since requesting remote image violates the policy of
    // a platform app, use navigator.sendBeacon() instead.
    window.ga('set', 'transport', 'beacon');

    // By default GA only accepts "http://" and "https://" protocol. Bypass the
    // check here since we are "chrome-extension://".
    window.ga('set', 'checkProtocolTask', null);
  })();
}

/**
 * Sends launch type event.
 * @param {boolean} ackMigrate Whether acknowledged to migrate during launch.
 */
function sendLaunchEvent(ackMigrate) {
  sendEvent({
    eventCategory: 'launch',
    eventAction: 'start',
    eventLabel: ackMigrate ? 'ack-migrate' : '',
  });
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
 * Sends capture type event.
 * @param {!Facing} facingMode Camera facing-mode of the capture.
 * @param {number} length Length of 1 minute buckets for captured video.
 * @param {!Resolution} resolution Capture resolution.
 * @param {!IntentResultType} intentResult
 */
function sendCaptureEvent(facingMode, length, resolution, intentResult) {
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
  sendEvent(
      {
        eventCategory: 'capture',
        eventAction: condState(Object.values(Mode)),
        eventLabel: facingMode,
        eventValue: length || 0,
      },
      new Map([
        // Skips 3rd dimension for obsolete 'sound' state.
        [4, condState([State.MIRROR])],
        [
          5,
          condState(
              [State.GRID_3x3, State.GRID_4x4, State.GRID_GOLDEN], State.GRID),
        ],
        [6, condState([State.TIMER_3SEC, State.TIMER_10SEC], State.TIMER)],
        [7, condState([State.MIC], Mode.VIDEO, true)],
        [8, condState([State.MAX_WND])],
        [9, condState([State.TALL])],
        [10, resolution.toString()],
        [11, condState([State.FPS_30, State.FPS_60], Mode.VIDEO, true)],
        [12, intentResult],
      ]));
}

/**
 * Sends perf type event.
 * @param {PerfEvent} event The target event type.
 * @param {number} duration The duration of the event in ms.
 * @param {Object=} extras Optional information for the event.
 */
function sendPerfEvent(event, duration, extras = {}) {
  const {resolution = '', facing = ''} = extras;
  sendEvent(
      {
        eventCategory: 'perf',
        eventAction: event,
        eventLabel: facing,
        // Round the duration here since GA expects that the value is an
        // integer. Reference:
        // https://support.google.com/analytics/answer/1033068
        eventValue: Math.round(duration),
      },
      new Map([
        [10, `${resolution}`],
      ]));
}

/**
 * Sends intent type event.
 * @param {!Intent} intent Intent to be logged.
 * @param {!IntentResultType} intentResult
 */
function sendIntentEvent(intent, intentResult) {
  const getBoolValue = (b) => b ? '1' : '0';
  sendEvent(
      {
        eventCategory: 'intent',
        eventAction: intent.mode,
        eventLabel: intentResult,
      },
      new Map([
        [12, intentResult],
        [13, getBoolValue(intent.shouldHandleResult)],
        [14, getBoolValue(intent.shouldDownScale)],
        [15, getBoolValue(intent.isSecure)],
      ]));
}

/**
 * Metrics types.
 * @enum {function(...)}
 */
export const Type = {
  LAUNCH: sendLaunchEvent,
  CAPTURE: sendCaptureEvent,
  PERF: sendPerfEvent,
  INTENT: sendIntentEvent,
};

/**
 * Logs the given metrics.
 * @param {!Type} type Metrics type.
 * @param {...*} args Optional rest parameters for logging metrics.
 * @return {!Promise}
 */
export async function log(type, ...args) {
  assert(window.ga !== null);
  assert(ready !== null);

  await ready;
  type(...args);
}
