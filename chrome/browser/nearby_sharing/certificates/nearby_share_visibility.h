// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_VISIBILITY_H_
#define CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_VISIBILITY_H_

// Specifies which contacts can receive the public certificate corresponding
// to the local device's private certificate.
enum class NearbyShareVisibility {
  kNoOne = 0,  // Integer values persisted in prefs; do not renumber.
  kAllContacts = 1,
  kSelectedContacts = 2
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_VISIBILITY_H_
