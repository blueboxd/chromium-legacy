// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTALLABLE_INSTALLABLE_UTILS_H_
#define CHROME_BROWSER_INSTALLABLE_INSTALLABLE_UTILS_H_

namespace content {
class BrowserContext;
}

class GURL;

// Returns true if there is an installed web app within |browser_context| that
// contains |url| within its scope, and false otherwise. For example, the URL
// https://example.com/a/b/c/d.html is contained within a web app with scope
// https://example.com/a/b/.
bool IsWebAppInstalledForUrl(content::BrowserContext* browser_context,
                             const GURL& url);

// Returns true if there is any installed web app within |browser_context|
// contained within |origin|. For example, a web app at https://example.com/a/b
// is contained within the origin https://example.com.
//
// |origin| is a GURL type for convenience; this method will DCHECK if
// |origin| != |origin.GetOrigin()|. Prefer using IsWebAppInstalledForUrl if a
// more specific URL is available.
bool DoesOriginContainAnyInstalledWebApp(
    content::BrowserContext* browser_context,
    const GURL& origin);

#endif  // CHROME_BROWSER_INSTALLABLE_INSTALLABLE_UTILS_H_
