// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for chrome-untrusted://telemetry_extension. */

/**
 * This is used to create TrustedScriptURL.
 * @type {!TrustedTypePolicy}
 */
const workerUrlPolicy = trustedTypes.createPolicy(
    'telemetry-extension-static',
    {createScriptURL: () => 'untrusted_worker.js'});

// Tests that web workers can be spawned from
// chrome-untrusted://telemetry_extension.
UNTRUSTED_TEST('UntrustedCanSpawnWorkers', async () => {
  if (!window.Worker) {
    throw 'Worker is not supported!';
  }

  // createScriptURL() always returns a 'untrusted_workjer.js' TrustedScriptURL,
  // so pass an empty string. In the future we might be able to avoid the empty
  // string if https://github.com/w3c/webappsec-trusted-types/issues/278 gets
  // fixed.
  /**
   * Closure Compiler only support string type as an argument to Worker
   * @suppress {checkTypes}
   */
  const worker = new Worker(workerUrlPolicy.createScriptURL(''));

  const workerResponse = new Promise((resolve, reject) => {
    /**
     * Registers onmessage event handler.
     * @param {MessageEvent} event Incoming message event.
     */
    worker.onmessage = function(event) {
      const data = /** @type {string} */ (event.data);
      resolve(data);
    };
    worker.onerror = function() {
      reject('There is an error with your worker');
    };
  });

  const MESSAGE = 'ping/pong message';

  worker.postMessage(MESSAGE);

  const response = /** @type {string} */ (await workerResponse);
  assertEquals(response, MESSAGE);
});

// Tests that TelemetryInfo throws an error if category is unknown.
UNTRUSTED_TEST('UntrustedRequestTelemetryInfoUnknownCategory', async () => {
  let caughtError = {};

  try {
    await chromeos.telemetry.probeTelemetryInfo(['unknown-category']);
  } catch (error) {
    caughtError = error;
  }

  assertEquals(caughtError.name, 'TypeError');
  assertEquals(
      caughtError.message,
      'Telemetry category \'unknown-category\' is unknown.');
});

// Tests that array of available routines can be successfully
// requested from chrome-untrusted://.
UNTRUSTED_TEST('UntrustedRequestAvailableRoutines', async () => {
  /** @type {!Array<!chromeos.health.mojom.DiagnosticRoutineEnum>} */
  const response = await chromeos.diagnostics.getAvailableRoutines();
  assertDeepEquals(response, [
    chromeos.health.mojom.DiagnosticRoutineEnum.kBatteryCapacity,
    chromeos.health.mojom.DiagnosticRoutineEnum.kBatteryHealth,
    chromeos.health.mojom.DiagnosticRoutineEnum.kUrandom,
    chromeos.health.mojom.DiagnosticRoutineEnum.kSmartctlCheck,
    chromeos.health.mojom.DiagnosticRoutineEnum.kAcPower,
    chromeos.health.mojom.DiagnosticRoutineEnum.kCpuCache,
    chromeos.health.mojom.DiagnosticRoutineEnum.kCpuStress,
    chromeos.health.mojom.DiagnosticRoutineEnum.kFloatingPointAccuracy,
    chromeos.health.mojom.DiagnosticRoutineEnum.kNvmeWearLevel,
    chromeos.health.mojom.DiagnosticRoutineEnum.kNvmeSelfTest,
    chromeos.health.mojom.DiagnosticRoutineEnum.kDiskRead,
    chromeos.health.mojom.DiagnosticRoutineEnum.kPrimeSearch,
    chromeos.health.mojom.DiagnosticRoutineEnum.kBatteryDischarge,
  ]);
});

// Tests that TelemetryInfo can be successfully requested from
// from chrome-untrusted://.
UNTRUSTED_TEST('UntrustedRequestTelemetryInfo', async () => {
  const response = await chromeos.telemetry.probeTelemetryInfo([
    'battery', 'non-removable-block-devices', 'cached-vpd-data', 'cpu',
    'timezone', 'memory', 'backlight', 'fan', 'stateful-partition', 'bluetooth'
  ]);

  assertDeepEquals(response, {
    batteryResult: {
      batteryInfo: {
        cycleCount: 100000000000000,
        voltageNow: 1234567890.123456,
        vendor: 'Google',
        serialNumber: 'abcdef',
        chargeFullDesign: 3000000000000000,
        chargeFull: 9000000000000000,
        voltageMinDesign: 1000000000.1001,
        modelName: 'Google Battery',
        chargeNow: 7777777777.777,
        currentNow: 0.9999999999999,
        technology: 'Li-ion',
        status: 'Charging',
        manufactureDate: '2020-07-30',
        temperature: 7777777777777777,
      }
    },
  });
});

// Tests that TelemetryInfo can be successfully requested from
// from chrome-untrusted://.
UNTRUSTED_TEST('UntrustedRequestTelemetryInfoWithInterceptor', async () => {
  const response = await chromeos.telemetry.probeTelemetryInfo([
    'battery', 'non-removable-block-devices', 'cached-vpd-data', 'cpu',
    'timezone', 'memory', 'backlight', 'fan', 'stateful-partition', 'bluetooth'
  ]);
  assertDeepEquals(response, {});
});
