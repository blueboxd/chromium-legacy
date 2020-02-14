// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permissions_client.h"

#include "base/callback.h"

namespace permissions {
namespace {
PermissionsClient* g_client = nullptr;
}

PermissionsClient::PermissionsClient() {
  DCHECK(!g_client);
  g_client = this;
}

PermissionsClient::~PermissionsClient() {
  g_client = nullptr;
}

// static
PermissionsClient* PermissionsClient::Get() {
  DCHECK(g_client);
  return g_client;
}

double PermissionsClient::GetSiteEngagementScore(
    content::BrowserContext* browser_context,
    const GURL& origin) {
  return 0.0;
}

void PermissionsClient::GetUkmSourceId(content::BrowserContext* browser_context,
                                       const content::WebContents* web_contents,
                                       const GURL& requesting_origin,
                                       GetUkmSourceIdCallback callback) {
  std::move(callback).Run(base::nullopt);
}

}  // namespace permissions
