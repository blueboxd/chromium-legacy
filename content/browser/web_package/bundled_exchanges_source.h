// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_SOURCE_H_
#define CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_SOURCE_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "content/common/content_export.h"

class GURL;

namespace content {

// A struct to abstract required information to access a BundledExchanges.
struct CONTENT_EXPORT BundledExchangesSource {
  // Constructs an invalid instance that does not match any.
  BundledExchangesSource();

  // Constructs a valid instance that match file URL for the given |file_path|.
  explicit BundledExchangesSource(const base::FilePath& file_path);

  // Copy constructor.
  explicit BundledExchangesSource(const BundledExchangesSource& src);

  // Checks if this instance is valid and can match a URL.
  bool IsValid() const;

  // Checks if the given |url| is for the BundledExchanges itself that this
  // instance represents. Note that this does not mean the |url| is for an
  // exchange provided by the BundledExchanges.
  bool Match(const GURL& url) const;

  // A flag to represent if this source can be trusted, i.e. using the URL in
  // the BundledExchanges as the origin for the content. Otherwise, we will use
  // the origin that serves the BundledExchanges itself. For instance, if the
  // BundledExchanges is in a local file system, file:// should be the origin.
  bool is_trusted = false;

  const base::FilePath file_path;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_SOURCE_H_
