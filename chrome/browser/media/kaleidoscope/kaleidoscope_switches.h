// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_SWITCHES_H_
#define CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_SWITCHES_H_

namespace base {
class CommandLine;
}  // namespace base

class GURL;

namespace switches {

extern const char kKaleidoscopeBackendUrl[];
extern const char kKaleidoscopeBackendUrlProdAlias[];
extern const char kKaleidoscopeBackendUrlProdUrl[];
extern const char kKaleidoscopeBackendUrlStagingAlias[];
extern const char kKaleidoscopeBackendUrlStagingUrl[];
extern const char kKaleidoscopeBackendUrlAutopushAlias[];
extern const char kKaleidoscopeBackendUrlAutopushUrl[];

}  // namespace switches

// Based on a |command_line| return the base URL to the Google API. The
// --kaleidoscope-backend-url switch takes either a URL or an alias which is
// one of autopush, staging or prod.
GURL GetGoogleAPIBaseURL(const base::CommandLine& command_line);

#endif  // CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_SWITCHES_H_
