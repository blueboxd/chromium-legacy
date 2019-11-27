// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.piet;

import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.piet.host.HostBindingProvider;
import org.chromium.chrome.browser.feed.library.sharedstream.offlinemonitor.StreamOfflineMonitor;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.BindingValue;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.HostBindingData;
import org.chromium.components.feed.core.proto.ui.stream.StreamOfflineExtensionProto.OfflineExtension;

/**
 * A Stream implementation of a {@link HostBindingProvider} which handles Stream host bindings and
 * can delegate to a host host binding provider if needed.
 */
public class PietHostBindingProvider extends HostBindingProvider {
    private static final String TAG = "PietHostBindingProvider";

    private final StreamOfflineMonitor mOfflineMonitor;
    /*@Nullable*/ private final HostBindingProvider mHostBindingProvider;

    public PietHostBindingProvider(
            /*@Nullable*/ HostBindingProvider hostHostBindingProvider,
            StreamOfflineMonitor offlineMonitor) {
        this.mHostBindingProvider = hostHostBindingProvider;
        this.mOfflineMonitor = offlineMonitor;
    }

    @Override
    public BindingValue getCustomElementDataBindingForValue(BindingValue bindingValue) {
        BindingValue genericBindingResult = getGenericBindingForValue(bindingValue);

        if (genericBindingResult != null) {
            return genericBindingResult;
        }

        if (mHostBindingProvider != null) {
            return mHostBindingProvider.getCustomElementDataBindingForValue(bindingValue);
        }
        return super.getCustomElementDataBindingForValue(bindingValue);
    }

    @Override
    public BindingValue getParameterizedTextBindingForValue(BindingValue bindingValue) {
        BindingValue genericBindingResult = getGenericBindingForValue(bindingValue);

        if (genericBindingResult != null) {
            return genericBindingResult;
        }

        if (mHostBindingProvider != null) {
            return mHostBindingProvider.getParameterizedTextBindingForValue(bindingValue);
        }
        return super.getParameterizedTextBindingForValue(bindingValue);
    }

    @Override
    public BindingValue getChunkedTextBindingForValue(BindingValue bindingValue) {
        BindingValue genericBindingResult = getGenericBindingForValue(bindingValue);

        if (genericBindingResult != null) {
            return genericBindingResult;
        }

        if (mHostBindingProvider != null) {
            return mHostBindingProvider.getChunkedTextBindingForValue(bindingValue);
        }
        return super.getChunkedTextBindingForValue(bindingValue);
    }

    @Override
    public BindingValue getImageBindingForValue(BindingValue bindingValue) {
        BindingValue genericBindingResult = getGenericBindingForValue(bindingValue);

        if (genericBindingResult != null) {
            return genericBindingResult;
        }

        if (mHostBindingProvider != null) {
            return mHostBindingProvider.getImageBindingForValue(bindingValue);
        }
        return super.getImageBindingForValue(bindingValue);
    }

    @Override
    public BindingValue getActionsBindingForValue(BindingValue bindingValue) {
        BindingValue genericBindingResult = getGenericBindingForValue(bindingValue);

        if (genericBindingResult != null) {
            return genericBindingResult;
        }

        if (mHostBindingProvider != null) {
            return mHostBindingProvider.getActionsBindingForValue(bindingValue);
        }
        return super.getActionsBindingForValue(bindingValue);
    }

    @Override
    public BindingValue getGridCellWidthBindingForValue(BindingValue bindingValue) {
        BindingValue genericBindingResult = getGenericBindingForValue(bindingValue);

        if (genericBindingResult != null) {
            return genericBindingResult;
        }

        if (mHostBindingProvider != null) {
            return mHostBindingProvider.getGridCellWidthBindingForValue(bindingValue);
        }
        return super.getGridCellWidthBindingForValue(bindingValue);
    }

    @Override
    public BindingValue getLogDataBindingForValue(BindingValue bindingValue) {
        BindingValue genericBindingResult = getGenericBindingForValue(bindingValue);

        if (genericBindingResult != null) {
            return genericBindingResult;
        }

        if (mHostBindingProvider != null) {
            return mHostBindingProvider.getLogDataBindingForValue(bindingValue);
        }
        return super.getLogDataBindingForValue(bindingValue);
    }

    @Override
    public BindingValue getTemplateBindingForValue(BindingValue bindingValue) {
        BindingValue genericBindingResult = getGenericBindingForValue(bindingValue);

        if (genericBindingResult != null) {
            return genericBindingResult;
        }

        if (mHostBindingProvider != null) {
            return mHostBindingProvider.getTemplateBindingForValue(bindingValue);
        }
        return super.getTemplateBindingForValue(bindingValue);
    }

    @Override
    public BindingValue getStyleBindingForValue(BindingValue bindingValue) {
        BindingValue genericBindingResult = getGenericBindingForValue(bindingValue);

        if (genericBindingResult != null) {
            return genericBindingResult;
        }

        if (mHostBindingProvider != null) {
            return mHostBindingProvider.getStyleBindingForValue(bindingValue);
        }
        return super.getStyleBindingForValue(bindingValue);
    }

    @Override
    public BindingValue getVisibilityBindingForValue(BindingValue bindingValue) {
        BindingValue genericBindingResult = getGenericBindingForValue(bindingValue);

        if (genericBindingResult != null) {
            return genericBindingResult;
        }

        if (mHostBindingProvider != null) {
            return mHostBindingProvider.getVisibilityBindingForValue(bindingValue);
        }

        return super.getVisibilityBindingForValue(bindingValue);
    }

    /**
     * Gets a {@link BindingValue} that supports multiple separate types. IE, Visibility or Style
     * bindings. Returns {@literal null} if no generic binding can be found.
     */
    /*@Nullable*/
    private BindingValue getGenericBindingForValue(BindingValue bindingValue) {
        HostBindingData hostBindingData = bindingValue.getHostBindingData();

        if (hostBindingData.hasExtension(OfflineExtension.offlineExtension)) {
            BindingValue result = getBindingForOfflineExtension(
                    hostBindingData.getExtension(OfflineExtension.offlineExtension));
            if (result != null) {
                return result;
            }
        }

        return null;
    }

    /*@Nullable*/
    private BindingValue getBindingForOfflineExtension(OfflineExtension offlineExtension) {
        if (!offlineExtension.hasUrl()) {
            Logger.e(TAG, "No URL for OfflineExtension, return clear.");
            return null;
        }

        return mOfflineMonitor.isAvailableOffline(offlineExtension.getUrl())
                ? offlineExtension.getOfflineBinding()
                : offlineExtension.getNotOfflineBinding();
    }
}
