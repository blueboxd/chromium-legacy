// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Message definitions passed over the TelemetryExtension
 * privileged/unprivileged pipe.
 */

/**
 * Enum for message types.
 * @enum {string}
 */
const Message = {
  DIAGNOSTICS_AVAILABLE_ROUTINES: 'DiagnosticsService.GetAvailableRoutines',
  PROBE_TELEMETRY_INFO: 'ProbeService.ProbeTelemetryInfo',
};


/**
 * Request message sent by the unprivileged context to request the privileged
 * context to diagnostics to get available routines.
 * @typedef {null}
 */
let DiagnosticsGetAvailableRoutinesRequest;

/**
 * Response message sent by the privileged context containing diagnostic
 * routine enums.
 * @typedef {{availableRoutines:
 *    !Array<!chromeos.health.mojom.DiagnosticRoutineEnum>}}
 */
let DiagnosticsGetAvailableRoutinesResponse;

/**
 * Request message sent by the unprivileged context to request the privileged
 * context to probe telemetry information
 * @typedef {null}
 */
let ProbeTelemetryInfoRequest;

/**
 * Response message sent by the privileged context sending telemetry
 * information.
 * @typedef {{telemetryInfo: chromeos.health.mojom.TelemetryInfo}}
 */
let ProbeTelemetryInfoResponse;
