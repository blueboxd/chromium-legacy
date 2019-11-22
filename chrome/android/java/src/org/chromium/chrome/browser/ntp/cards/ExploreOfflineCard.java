// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.view.View;
import android.view.ViewStub;
import android.widget.Button;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.net.NetworkChangeNotifier;

/**
 * Shows a card prompting the user to view offline content when they are offline. This class is
 * responsible for inflating the card layout and displaying the card based on network connectivity.
 */
public class ExploreOfflineCard {
    private final View mRootView;
    private final Runnable mOpenDownloadHomeCallback;
    private NetworkChangeNotifier.ConnectionTypeObserver mConnectionTypeObserver;
    private boolean mCardDismissed;

    /**
     * Constructor.
     * @param rootView The parent view where this card should be inflated.
     * @param openDownloadHomeCallback A callback to open download home.
     */
    public ExploreOfflineCard(View rootView, Runnable openDownloadHomeCallback) {
        mRootView = rootView;
        mOpenDownloadHomeCallback = openDownloadHomeCallback;

        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.OFFLINE_HOME)) return;
        setCardViewVisibility();
        mConnectionTypeObserver = connectionType -> {
            setCardViewVisibility();
        };

        NetworkChangeNotifier.addConnectionTypeObserver(mConnectionTypeObserver);
    }

    /** Called during destruction of the view. */
    public void destroy() {
        NetworkChangeNotifier.removeConnectionTypeObserver(mConnectionTypeObserver);
    }

    private void setCardViewVisibility() {
        boolean isVisible = !mCardDismissed && DownloadUtils.shouldShowOfflineHome();
        View cardView = mRootView.findViewById(R.id.explore_offline_card);

        if (cardView == null) {
            if (!isVisible) return;
            cardView = inflateCardLayout();
        }
        cardView.setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    private View inflateCardLayout() {
        View cardView =
                ((ViewStub) mRootView.findViewById(R.id.explore_offline_card_stub)).inflate();
        Button confirmButton = cardView.findViewById(R.id.button_primary);
        Button cancelButton = cardView.findViewById(R.id.button_secondary);

        confirmButton.setOnClickListener(v -> {
            mCardDismissed = true;
            setCardViewVisibility();
            mOpenDownloadHomeCallback.run();
        });
        cancelButton.setOnClickListener(v -> {
            mCardDismissed = true;
            setCardViewVisibility();
        });
        return cardView;
    }
}
