// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.content_public.browser.InterfaceRegistrar;
import org.chromium.content_public.browser.WebContents;
import org.chromium.services.service_manager.InterfaceRegistry;
import org.chromium.webshare.mojom.ShareService;

/**
 * Registers Java implementations of mojo interfaces.
 */
class MojoInterfaceRegistrar {
    @CalledByNative
    private static void registerMojoInterfaces() {
        InterfaceRegistrar.Registry.addWebContentsRegistrar(new WebContentsInterfaceRegistrar());
    }

    private static class WebContentsInterfaceRegistrar implements InterfaceRegistrar<WebContents> {
        @Override
        public void registerInterfaces(InterfaceRegistry registry, final WebContents webContents) {
            registry.addInterface(ShareService.MANAGER, new WebShareServiceFactory(webContents));
        }
    }
}
