// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MEDIA_CDM_MANIFEST_H_
#define CHROME_COMMON_MEDIA_CDM_MANIFEST_H_

namespace base {
class Value;
}

namespace content {
struct CdmCapability;
}

// Returns whether the CDM's API versions, as specified in the manifest, are
// supported in this Chrome binary and not disabled at run time.
// Checks the module API, CDM interface API, and Host API.
// This should never fail except in rare cases where the component has not been
// updated recently or the user downgrades Chrome.
bool IsCdmManifestCompatibleWithChrome(const base::Value& manifest);

// Extracts the necessary information from |manifest| and updates |capability|.
// Returns true on success, false if there are errors in the manifest.
// If this method returns false, |capability| may or may not be updated.
bool ParseCdmManifest(const base::Value& manifest,
                      content::CdmCapability* capability);

#endif  // CHROME_COMMON_MEDIA_CDM_MANIFEST_H_
