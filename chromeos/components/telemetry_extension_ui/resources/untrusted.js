// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 *
 * Diagnostic Processor Support Library (DPSL) is a collection of telemetry and
 * diagnostics interfaces exposed to third-parties:
 *
 *   - chromeos.diagnostics
 *     | Diagnostics interface for running device diagnostics routines (tests).
 *
 *   - chromeos.telemetry
 *     | Telemetry (a.k.a. Probe) interface for getting device telemetry
 *     | information.
 */

chromeos.diagnostics = null;

chromeos.telemetry = null;

/**
 * This is only for testing purposes. Please don't use it in the production,
 * because we may silently change or remove it.
 */
chromeos.test_support = {};

(() => {
  const messagePipe =
      new MessagePipe('chrome://telemetry-extension', window.parent);

  /**
   * DPSL Diagnostics Requester.
   */
  class DiagnosticsRequester {
    constructor() {}

    /**
     * Requests a list of available routines.
     * @return {!Promise<!Array<!chromeos.health.mojom.DiagnosticRoutineEnum>>}
     * @public
     */
    async getAvailableRoutines() {
      const response =
          /** @type {dpsl_internal.DiagnosticsGetAvailableRoutinesResponse} */ (
              await messagePipe.sendMessage(
                  dpsl_internal.Message.DIAGNOSTICS_AVAILABLE_ROUTINES));
      return response.availableRoutines;
    }
  };

  /**
   * DPSL Telemetry Requester.
   */
  class TelemetryRequester {
    constructor() {}

    /**
     * Requests telemetry info.
     * @param { !Array<!string> } categories
     * @return { !Promise<!chromeos.health.mojom.TelemetryInfo> }
     * @public
     */
    async probeTelemetryInfo(categories) {
      const response =
          /** @type {dpsl_internal.ProbeTelemetryInfoResponse} */ (
              await messagePipe.sendMessage(
                  dpsl_internal.Message.PROBE_TELEMETRY_INFO, categories));
      if (response.error !== undefined) {
        throw response.error;
      }
      return /** @type {!chromeos.health.mojom.TelemetryInfo} */ (
          response.telemetryInfo);
    }
  };

  globalThis.chromeos.diagnostics = new DiagnosticsRequester();
  globalThis.chromeos.telemetry = new TelemetryRequester();

  globalThis.chromeos.test_support.messagePipe = function() {
    console.warn(
        'messagePipe() is a method for testing purposes only. Please',
        'do not use it, otherwise your app may be broken in the future.');
    return messagePipe;
  }
})();
