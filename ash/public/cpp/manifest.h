// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_MANIFEST_H_
#define ASH_PUBLIC_CPP_MANIFEST_H_

#include "services/service_manager/public/cpp/manifest.h"

namespace ash {

// Returns the service manifest used for the ash service in a production
// environment.
const service_manager::Manifest& GetManifest();

// Sets a global overlay with which to amend the manifest returned by
// |GetManifest()|. Must be called before the first call to |GetManifest()| in
// order to have any effect. This is useful for e.g. browser test environments
// where |GetManifest()| is called by production code and the test environment
// needs to affect its behavior with test-only dependencies.
void AmendManifestForTesting(const service_manager::Manifest& amendment);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_MANIFEST_H_
