// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let diagnosticsService = null;
let probeService = null;

/**
 * Lazy creates pointer to remote implementation of diagnostics service.
 * @return {!chromeos.health.mojom.DiagnosticsServiceRemote}
 */
function getOrCreateDiagnosticsService() {
  if (diagnosticsService === null) {
    diagnosticsService = chromeos.health.mojom.DiagnosticsService.getRemote();
  }
  return /** @type {!chromeos.health.mojom.DiagnosticsServiceRemote} */ (
      diagnosticsService);
}

/**
 * Lazy creates pointer to remote implementation of probe service.
 * @return {!chromeos.health.mojom.ProbeServiceRemote}
 */
function getOrCreateProbeService() {
  if (probeService === null) {
    probeService = chromeos.health.mojom.ProbeService.getRemote();
  }
  return /** @type {!chromeos.health.mojom.ProbeServiceRemote} */ (
      probeService);
}

/**
 * Proxying telemetry requests between TelemetryRequester on
 * chrome-untrusted:// side with WebIDL types and ProbeService on chrome://
 * side with Mojo types.
 */
class TelemetryProxy {
  constructor() {
    const categoryEnum = chromeos.health.mojom.ProbeCategoryEnum;

    /**
     * @type { !Map<!string, !chromeos.health.mojom.ProbeCategoryEnum> }
     * @const
     */
    this.categoryToEnum_ = new Map([
      ['battery', categoryEnum.kBattery],
      ['non-removable-block-devices', categoryEnum.kNonRemovableBlockDevices],
      ['cached-vpd-data', categoryEnum.kCachedVpdData],
      ['cpu', categoryEnum.kCpu], ['timezone', categoryEnum.kTimezone],
      ['memory', categoryEnum.kMemory], ['backlight', categoryEnum.kBacklight],
      ['fan', categoryEnum.kFan],
      ['stateful-partition', categoryEnum.kStatefulPartition],
      ['bluetooth', categoryEnum.kBluetooth]
    ]);
  }

  /**
   * @param { !Array<!string> } categories
   * @return { !Array<!chromeos.health.mojom.ProbeCategoryEnum> }
   */
  convertCategories(categories) {
    return categories.map((category) => {
      if (!this.categoryToEnum_.has(category)) {
        throw TypeError(`Telemetry category '${category}' is unknown.`);
      }
      return this.categoryToEnum_.get(category);
    });
  }

  /**
   * This method converts Mojo types to WebIDL types applying next rules:
   *   1. convert objects like { value: X } to X, where X is a number;
   *   2. omit null/undefined properties;
   *   3. convert objects without properties to null.
   * @param {?Object|string|number|null|undefined} input
   * @return {?Object|string|number|null}
   */
  convert(input) {
    // After this closure compiler knows that input is not null.
    if (input === null || typeof input === 'undefined') {
      return null;
    }

    // After this closure compiler knows that input is {!Object}.
    if (typeof input !== 'object') {
      return input;
    }

    // 1 rule: convert objects like { value: X } to X, where X is a number.
    if (Object.entries(input).length === 1 &&
        typeof input['value'] === 'number') {
      return input['value'];
    }

    let output = {};
    Object.entries(input).forEach(kv => {
      const key = /** @type {!string} */ (kv[0]);
      const value = /** @type {?Object|string|number|null|undefined} */ (kv[1]);
      const converted = this.convert(value);

      // 2 rule: omit null/undefined properties.
      if (converted !== null && typeof converted !== 'undefined') {
        output[key] = converted;
      }
    });

    // 3 rule. convert objects without properties to null.
    if (Object.entries(output).length === 0) {
      return null;
    }
    return output;
  };

  /**
   * Requests telemetry info.
   * @param { Object } message
   * @return { !Object }
   */
  async handleProbeTelemetryInfo(message) {
    const request =
        /** @type {!dpsl_internal.ProbeTelemetryInfoRequest} */ (message);

    /** @type {!Array<!chromeos.health.mojom.ProbeCategoryEnum>} */
    let categories = [];
    try {
      categories = this.convertCategories(request);
    } catch (/** @type {!Error}*/ error) {
      return {error};
    }

    const telemetryInfo =
        await getOrCreateProbeService().probeTelemetryInfo(categories);
    return /** @type {Object} */ (this.convert(telemetryInfo)) ||
        {telemetryInfo: {}};
  }
};

const telemetryProxy = new TelemetryProxy();

/**
 * @type { MessagePipe|null }
 */
var untrustedMessagePipe = null;

document.addEventListener('DOMContentLoaded', async () => {
  const untrustedMessagePipe =
      new MessagePipe('chrome-untrusted://telemetry-extension');

  untrustedMessagePipe.registerHandler(
      dpsl_internal.Message.DIAGNOSTICS_AVAILABLE_ROUTINES, async () => {
        return await getOrCreateDiagnosticsService().getAvailableRoutines();
      });

  untrustedMessagePipe.registerHandler(
      dpsl_internal.Message.PROBE_TELEMETRY_INFO,
      (message) => telemetryProxy.handleProbeTelemetryInfo(message));

  globalThis.untrustedMessagePipe = untrustedMessagePipe;
});
