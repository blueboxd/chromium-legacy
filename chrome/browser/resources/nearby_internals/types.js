// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Severity enum based on LogMessage format. Needs to stay in sync with the
 * NearbyInternalsLogsHandler.
 * @enum {number}
 */
export const Severity = {
  INFO: 0,
  WARNING: 1,
  ERROR: 2,
  VERBOSE: 3
};

/**
 * The type of log message object. The definition is based on
 * chrome/browser/ui/webui/nearby_internals/nearby_internals_logs_handler.cc:
 * LogMessageToDictionary()
 * @typedef {{text: string,
 *            time: string,
 *            file: string,
 *            line: number,
 *            severity: Severity}}
 */
export let LogMessage;

/**
 * RPC enum based on the HTTP request/response object. Needs to stay in sync
 * with the NearbyInternalsHttpHandler C++ code, defined in
 * chrome/browser/ui/webui/nearby_internals/nearby_internals_http_handler.cc.
 * @enum {number}
 */
export const Rpc = {
  CERTIFICATE: 0,
  CONTACT: 1,
  DEVICE: 2
};

/**
 * Direction enum based on the HTTP request/response object. Needs to stay in
 * sync with the NearbyInternalsHttpHandler C++ code, defined in
 * chrome/browser/ui/webui/nearby_internals/nearby_internals_http_handler.cc.
 * @enum {number}
 */
export const Direction = {
  REQUEST: 0,
  RESPONSE: 1
};

/**
 * The HTTP request/response object, sent by NearbyInternalsHttpHandler
 * chrome/browser/ui/webui/nearby_internals/nearby_internals_http_handler.cc.
 * @typedef {{body: string,
 *            time: string,
 *            rpc: !Rpc,
 *            direction: !Direction}}
 */
export let HttpMessage;
