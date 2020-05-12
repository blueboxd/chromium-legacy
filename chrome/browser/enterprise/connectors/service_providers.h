// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_SERVICE_PROVIDERS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_SERVICE_PROVIDERS_H_

namespace component_updater {
class ComponentUpdateService;
}

namespace enterprise_connectors {

// Registers a component extension with the update service which downloads
// and verifies service provider configurations used by enterprise connectors.
void RegisterServiceProvidersComponent(
    component_updater::ComponentUpdateService* cus);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_SERVICE_PROVIDERS_H_
