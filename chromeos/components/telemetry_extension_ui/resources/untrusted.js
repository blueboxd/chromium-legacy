// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** A pipe through which we can send messages to the parent frame. */
const parentMessagePipe =
    new MessagePipe('chrome://telemetry-extension', window.parent);

/**
 * Requests probe telemetry info.
 * @return {!Promise<ProbeTelemetryInfoResponse>}
 */
async function requestTelemetryInfo() {
  const response = /** @type {!ProbeTelemetryInfoResponse} */ (
      await parentMessagePipe.sendMessage(Message.PROBE_TELEMETRY_INFO));
  return response;
}
