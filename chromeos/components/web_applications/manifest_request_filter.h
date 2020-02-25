// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_WEB_APPLICATIONS_MANIFEST_REQUEST_FILTER_H_
#define CHROMEOS_COMPONENTS_WEB_APPLICATIONS_MANIFEST_REQUEST_FILTER_H_

namespace content {
class WebUIDataSource;
}

namespace web_app {

// Calls |source->SetRequestFilter()| to set up respones to requests for
// "manifest.json" while replacing $i18nRaw{name} in the contents indicated by
// |manifest_idr| with the name from |name_ids|.
void SetManifestRequestFilter(content::WebUIDataSource* source,
                              int manifest_idr,
                              int name_ids);

}  // namespace web_app

#endif  // CHROMEOS_COMPONENTS_WEB_APPLICATIONS_MANIFEST_REQUEST_FILTER_H_
