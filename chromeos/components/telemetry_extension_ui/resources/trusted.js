// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Pointer to remote implementation of diagnostics service.
 * @type {!chromeos.health.mojom.DiagnosticsServiceRemote}
 */
const diagnosticsService = chromeos.health.mojom.DiagnosticsService.getRemote();

/**
 * Pointer to remote implementation of probe service.
 * @type {!chromeos.health.mojom.ProbeServiceRemote}
 */
const probeService = chromeos.health.mojom.ProbeService.getRemote();

const untrustedMessagePipe =
  new MessagePipe('chrome-untrusted://telemetry-extension');

untrustedMessagePipe.registerHandler(Message.DIAGNOSTICS_AVAILABLE_ROUTINES,
  async () => {
    return await diagnosticsService.getAvailableRoutines();
  });

untrustedMessagePipe.registerHandler(Message.PROBE_TELEMETRY_INFO, async () => {
  const response = await probeService.probeTelemetryInfo(
    [chromeos.health.mojom.ProbeCategoryEnum.kBattery]);
  return { telemetryInfo: response.telemetryInfo };
});
