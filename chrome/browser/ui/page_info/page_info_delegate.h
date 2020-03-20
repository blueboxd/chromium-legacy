// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_DELEGATE_H_
#define CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_DELEGATE_H_

#include <string>
#include "components/content_settings/core/common/content_settings_types.h"

// PageInfoDelegate allows an embedder to customize PageInfo logic.
class PageInfoDelegate {
 public:
  virtual ~PageInfoDelegate() = default;

  // Whether the content setting of type |type| has changed via Page Info UI.
  virtual bool HasContentSettingChangedViaPageInfo(
      ContentSettingsType type) = 0;
  virtual int GetFirstPartyAllowedCookiesCount(const GURL& site_url) = 0;
  virtual int GetFirstPartyBlockedCookiesCount(const GURL& site_url) = 0;
  virtual int GetThirdPartyAllowedCookiesCount(const GURL& site_url) = 0;
  virtual int GetThirdPartyBlockedCookiesCount(const GURL& site_url) = 0;
};

#endif  // CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_DELEGATE_H_
