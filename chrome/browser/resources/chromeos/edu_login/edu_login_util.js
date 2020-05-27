// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Information for a parent account.
 * @typedef {{
 *   email: string,
 *   displayName: string,
 *   profileImage: string,
 *   obfuscatedGaiaId: string
 * }}
 */
export let ParentAccount;

/**
 * Failure result of parentSignin call.
 * @typedef {{
 *   isWrongPassword: boolean
 * }}
 */
export let ParentSigninFailureResult;

/**
 * Additional EDU-specific params for 'completeLogin' call.
 * @typedef {{
 *   reAuthProofToken: string,
 *   parentObfuscatedGaiaId: string,
 * }}
 */
export let EduLoginParams;

// Keep in sync with
// chrome/browser/ui/webui/signin/inline_login_handler_dialog_chromeos.h
/** @enum {number} */
export const EduCoexistenceFlowResult = {
  PARENTS_LIST_SCREEN: 0,
  PARENT_PASSWORD_SCREEN: 1,
  PARENT_INFO_SCREEN1: 2,
  PARENT_INFO_SCREEN2: 3,
  EDU_ACCOUNT_LOGIN_SCREEN: 4,
  FLOW_COMPLETED: 5,
};
