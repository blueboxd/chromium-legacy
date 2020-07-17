
// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for interacting with Network Diagnostics.
 */

/**
 * A network diagnostics routine. Holds descriptive information about the
 * routine, and it's transient state.
 * @typedef {{
 *   name: string,
 *   type: !RoutineType,
 *   running: boolean,
 *   resultMsg: string,
 * }}
 */
let Routine;

/**
 * A routine response from the Network Diagnostics mojo service.
 * @typedef {{
 *   verdict: chromeos.networkDiagnostics.mojom.RoutineVerdict
 * }}
 */
let RoutineResponse;

/**
 * Definition for all Network diagnostic routine types. This enum is intended
 * to be used as an index in an array of routines.
 * @enum {number}
 */
const RoutineType = {
  LAN_CONNECTIVITY: 0,
  SIGNAL_STRENGTH: 1,
  GATEWAY_PING: 2,
  SECURE_WIFI: 3,
  DNS_RESOLVER: 4,
  DNS_LATENCY: 5,
  DNS_RESOLUTION: 6,
};

/**
 * Helper function to create a routine object.
 * @param {string} name
 * @param {!RoutineType} type
 * @return {!Routine} Routine object
 */
function createRoutine(name, type) {
  return {name: name, type: type, running: false, resultMsg: ''};
}

Polymer({
  is: 'network-diagnostics',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /**
     * List of Diagnostics Routines
     * @private {!Array<!Routine>}
     */
    routines_: {
      type: Array,
      value: function() {
        const routines = [];
        routines[RoutineType.LAN_CONNECTIVITY] = createRoutine(
            'NetworkDiagnosticsLanConnectivity', RoutineType.LAN_CONNECTIVITY);
        routines[RoutineType.SIGNAL_STRENGTH] = createRoutine(
            'NetworkDiagnosticsSignalStrength', RoutineType.SIGNAL_STRENGTH);
        routines[RoutineType.GATEWAY_PING] = createRoutine(
            'NetworkDiagnosticsGatewayCanBePinged', RoutineType.GATEWAY_PING);
        routines[RoutineType.SECURE_WIFI] = createRoutine(
            'NetworkDiagnosticsHasSecureWiFiConnection',
            RoutineType.SECURE_WIFI);
        routines[RoutineType.DNS_RESOLVER] = createRoutine(
            'NetworkDiagnosticsDnsResolverPresent', RoutineType.DNS_RESOLVER);
        routines[RoutineType.DNS_LATENCY] = createRoutine(
            'NetworkDiagnosticsDnsLatency', RoutineType.DNS_LATENCY);
        routines[RoutineType.DNS_RESOLUTION] = createRoutine(
            'NetworkDiagnosticsDnsResolution', RoutineType.DNS_RESOLUTION);
        return routines;
      }
    }
  },

  /**
   * Network Diagnostics mojo remote.
   * @private {
   *     ?chromeos.networkDiagnostics.mojom.NetworkDiagnosticsRoutinesRemote}
   */
  networkDiagnostics_: null,

  /** @override */
  created() {
    this.networkDiagnostics_ = chromeos.networkDiagnostics.mojom
                                   .NetworkDiagnosticsRoutines.getRemote();
  },

  /** @private */
  onRunAllRoutinesClick_() {
    for (const routine of this.routines_) {
      this.runRoutine_(routine.type);
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  onRunRoutineClick_(event) {
    this.runRoutine_(event.model.index);
  },

  /**
   * @param {!RoutineType} type
   * @private
   */
  runRoutine_(type) {
    this.set(`routines_.${type}.running`, true);
    this.set(`routines_.${type}.resultMsg`, '');
    const element =
        this.shadowRoot.querySelectorAll('.routine-container')[type];
    element.classList.remove('result-passed', 'result-error', 'result-not-run');
    switch (type) {
      case RoutineType.LAN_CONNECTIVITY:
        this.networkDiagnostics_.lanConnectivity().then(
            result => this.evaluateRoutine_(type, result));
        break;
      case RoutineType.SIGNAL_STRENGTH:
        this.networkDiagnostics_.signalStrength().then(
            result => this.evaluateRoutine_(type, result));
        break;
      case RoutineType.GATEWAY_PING:
        this.networkDiagnostics_.gatewayCanBePinged().then(
            result => this.evaluateRoutine_(type, result));
        break;
      case RoutineType.SECURE_WIFI:
        this.networkDiagnostics_.hasSecureWiFiConnection().then(
            result => this.evaluateRoutine_(type, result));
        break;
      case RoutineType.DNS_RESOLVER:
        this.networkDiagnostics_.dnsResolverPresent().then(
            result => this.evaluateRoutine_(type, result));
        break;
      case RoutineType.DNS_LATENCY:
        this.networkDiagnostics_.dnsLatency().then(
            result => this.evaluateRoutine_(type, result));
        break;
      case RoutineType.DNS_RESOLUTION:
        this.networkDiagnostics_.dnsResolution().then(
            result => this.evaluateRoutine_(type, result));
        break;
    }
  },

  /**
   * @param {!RoutineType} type
   * @param {!RoutineResponse} result
   * @private
   */
  evaluateRoutine_(type, result) {
    const routine = `routines_.${type}`;
    this.set(`${routine}.running`, false);

    const element =
        this.shadowRoot.querySelectorAll('.routine-container')[type];
    let resultMsg = '';

    switch (result.verdict) {
      case chromeos.networkDiagnostics.mojom.RoutineVerdict.kNoProblem:
        resultMsg = this.i18n('NetworkDiagnosticsPassed');
        element.classList.add('result-passed');
        break;
      case chromeos.networkDiagnostics.mojom.RoutineVerdict.kProblem:
        resultMsg = this.i18n('NetworkDiagnosticsFailed');
        element.classList.add('result-error');
        break;
      case chromeos.networkDiagnostics.mojom.RoutineVerdict.kNotRun:
        resultMsg = this.i18n('NetworkDiagnosticsNotRun');
        element.classList.add('result-not-run');
        break;
    }

    this.set(routine + '.resultMsg', resultMsg);
  },
});
