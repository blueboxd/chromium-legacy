// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {string} email
 * @param {string} displayName
 * @param {string} profileImage
 * @param {string} obfuscatedGaiaId
 * @return {ParentAccount}
 */
export function getFakeParent(
    email, displayName, profileImage, obfuscatedGaiaId) {
  return {
    email: email,
    displayName: displayName,
    profileImage: profileImage,
    obfuscatedGaiaId: obfuscatedGaiaId,
  };
}
