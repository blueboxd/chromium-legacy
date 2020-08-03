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

// Tests that TelemetryInfo can be successfully requested from
// from chrome-untrusted://.
UNTRUSTED_TEST('UntrustedRequestTelemetryInfo', async () => {
  /** @type {!ProbeTelemetryInfoResponse} */
  const response = await requestTelemetryInfo();
  assertDeepEquals(response, {
    'telemetryInfo': {
      'backlightResult': null,
      'batteryResult': null,
      'blockDeviceResult': null,
      'bluetoothResult': null,
      'cpuResult': null,
      'fanResult': null,
      'memoryResult': null,
      'statefulPartitionResult': null,
      'timezoneResult': null,
      'vpdResult': null,
    }
  });
});

// Tests that array of available routines can be successfully
// requested from chrome-untrusted://.
UNTRUSTED_TEST('UntrustedRequestAvailableRoutines', async () => {
  /** @type {!DiagnosticsGetAvailableRoutinesResponse} */
  const response = await getAvailableRoutines();
  assertDeepEquals(response, {
    'availableRoutines': [
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
    ]
  });
});
