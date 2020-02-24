// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is tool-generated using pedal_processor.  Do not edit.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PEDAL_CONCEPTS_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PEDAL_CONCEPTS_H_

// This value is generated during Pedal concept data processing, and written
// to all data files as well as the source code here to ensure synchrony.
// The runtime loaded data must match this version exactly or it won't load.
constexpr int OMNIBOX_PEDAL_CONCEPTS_DATA_VERSION = 15220441;

// Unique identifiers for Pedals, used to bind loaded data to implementations.
enum class OmniboxPedalId {
  NONE = 0,

  CLEAR_BROWSING_DATA = 1,
  CHANGE_SEARCH_ENGINE = 2,
  MANAGE_PASSWORDS = 3,
  CHANGE_HOME_PAGE = 4,
  UPDATE_CREDIT_CARD = 5,
  LAUNCH_INCOGNITO = 6,
  TRANSLATE = 7,
  UPDATE_CHROME = 8,
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PEDAL_CONCEPTS_H_
