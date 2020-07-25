// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const ClientVariations = goog.require('proto.variations.ClientVariations');

/**
 * Parses a serialized ClientVariations proto into a human-readable format.
 * @param {string} data The serialized ClientVariations proto contents, e.g.
 *     taken from the X-Client-Data header.
 * @return {{
 *   variationIds: !Array<number>,
 *   triggerVariationIds: !Array<number>,
 * }}
 */
export function parse(data) {
  let decoded = '';
  try {
    decoded = atob(data);
  } catch (e) {
    // Nothing to do here -- it's fine to leave `decoded` empty if base64
    // decoding fails.
  }

  const bytes = [];
  for (let i = 0; i < decoded.length; i++) {
    bytes.push(decoded.charCodeAt(i));
  }


  const parsed = ClientVariations.deserializeBinary(bytes);
  return {
    'variationIds': parsed.getVariationIdList(),
    'triggerVariationIds': parsed.getTriggerVariationIdList(),
  };
}

goog.exportSymbol('parseClientVariations', parse);
