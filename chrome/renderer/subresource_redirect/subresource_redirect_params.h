// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_PARAMS_H_
#define CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_PARAMS_H_

#include <string>

namespace subresource_redirect {

// Returns true if Subresource Redirect is forced to be enabled from the
// command line.
bool ShouldForceEnableSubresourceRedirect();

// Returns kLitePagesServerSubresourceHost if set, else returns the default.
std::string LitePageSubresourceHost();

}  // namespace subresource_redirect

#endif  // CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_PARAMS_H_
