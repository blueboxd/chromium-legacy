// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * This file contains typedefs properties for CrNetworkList, shared by
 * CrNetworkListItem.
 */

const CrNetworkList = {};

/**
 * Custom data for implementation specific network list items.
 * @typedef {{
 *   customItemName: string,
 *   polymerIcon: (string|undefined),
 *   customData: (!Object|undefined),
 *   showBeforeNetworksList: boolean,
 * }}
 */
CrNetworkList.CustomItemState;

/** @typedef {OncMojo.NetworkStateProperties|CrNetworkList.CustomItemState} */
CrNetworkList.CrNetworkListItemType;
