// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const probeService = chromeos.health.mojom.ProbeService.getRemote();

const untrustedMessagePipe =
    new MessagePipe('chrome-untrusted://telemetry-extension');

untrustedMessagePipe.registerHandler(Message.PROBE_TELEMETRY_INFO, async () => {
  const response = await probeService.probeTelemetryInfo(
      [chromeos.health.mojom.ProbeCategoryEnum.kBattery]);
  return {telemetryInfo: response.telemetryInfo};
});
