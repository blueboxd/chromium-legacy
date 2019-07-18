// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import android.accounts.Account;
import android.support.annotation.NonNull;

import org.chromium.components.signin.CoreAccountId;
import org.chromium.components.signin.CoreAccountInfo;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * This class is used by the {@link FakeAccountManagerDelegate} to hold information about a given
 * account, such as its password and set of granted auth tokens.
 */
public class AccountHolder {
    private final CoreAccountInfo mAccountInfo;
    private final Map<String, String> mAuthTokens;
    private final Map<String, Boolean> mHasBeenAccepted;
    private final boolean mAlwaysAccept;
    private final Set<String> mFeatures;

    private AccountHolder(CoreAccountInfo accountInfo, Map<String, String> authTokens,
            Map<String, Boolean> hasBeenAccepted, boolean alwaysAccept, Set<String> features) {
        assert accountInfo != null;
        assert authTokens != null;
        assert hasBeenAccepted != null;
        assert features != null;

        mAccountInfo = accountInfo;
        mAuthTokens = authTokens;
        mHasBeenAccepted = hasBeenAccepted;
        mAlwaysAccept = alwaysAccept;
        mFeatures = features;
    }

    public CoreAccountInfo getAccountInfo() {
        return mAccountInfo;
    }

    public boolean hasAuthTokenRegistered(String authTokenType) {
        return mAuthTokens.containsKey(authTokenType);
    }

    public String getAuthToken(String authTokenType) {
        return mAuthTokens.get(authTokenType);
    }

    public boolean hasBeenAccepted(String authTokenType) {
        return mAlwaysAccept
                || mHasBeenAccepted.containsKey(authTokenType)
                && mHasBeenAccepted.get(authTokenType);
    }

    /**
     * Removes an auth token from the auth token map.
     *
     * @param authToken the auth token to remove
     * @return true if the auth token was found
     */
    public boolean removeAuthToken(String authToken) {
        String foundKey = null;
        for (Map.Entry<String, String> tokenEntry : mAuthTokens.entrySet()) {
            if (authToken.equals(tokenEntry.getValue())) {
                foundKey = tokenEntry.getKey();
                break;
            }
        }
        if (foundKey == null) {
            return false;
        } else {
            mAuthTokens.remove(foundKey);
            return true;
        }
    }

    public Set<String> getFeatures() {
        return mFeatures;
    }

    @Override
    public int hashCode() {
        return mAccountInfo.hashCode();
    }

    @Override
    public boolean equals(Object that) {
        return that instanceof AccountHolder
                && mAccountInfo.equals(((AccountHolder) that).getAccountInfo());
    }

    public static Builder builder(@NonNull Account account) {
        return new Builder(account);
    }

    public AccountHolder withAuthTokens(Map<String, String> authTokens) {
        return copy().authTokens(authTokens).build();
    }

    public AccountHolder withAuthToken(String authTokenType, String authToken) {
        return copy().authToken(authTokenType, authToken).build();
    }

    public AccountHolder withHasBeenAccepted(String authTokenType, boolean hasBeenAccepted) {
        return copy().hasBeenAccepted(authTokenType, hasBeenAccepted).build();
    }

    public AccountHolder withAlwaysAccept(boolean alwaysAccept) {
        return copy().alwaysAccept(alwaysAccept).build();
    }

    private Builder copy() {
        return builder(mAccountInfo.getAccount())
                .accountId(mAccountInfo.getId())
                .authTokens(mAuthTokens)
                .hasBeenAcceptedMap(mHasBeenAccepted)
                .alwaysAccept(mAlwaysAccept);
    }

    /**
     * Used to construct AccountHolder instances.
     */
    public static class Builder {
        private Account mAccount;
        private CoreAccountId mAccountId;
        private Map<String, String> mAuthTokens = new HashMap<>();
        private Map<String, Boolean> mHasBeenAccepted = new HashMap<>();
        private boolean mAlwaysAccept;
        private Set<String> mFeatures = new HashSet<>();

        public Builder(@NonNull Account account) {
            mAccount = account;
        }

        public Builder account(@NonNull Account account) {
            mAccount = account;
            return this;
        }

        public Builder accountId(@NonNull CoreAccountId accountId) {
            mAccountId = accountId;
            return this;
        }

        public Builder authToken(String authTokenType, String authToken) {
            mAuthTokens.put(authTokenType, authToken);
            return this;
        }

        public Builder authTokens(@NonNull Map<String, String> authTokens) {
            mAuthTokens = authTokens;
            return this;
        }

        public Builder hasBeenAccepted(String authTokenType, boolean hasBeenAccepted) {
            mHasBeenAccepted.put(authTokenType, hasBeenAccepted);
            return this;
        }

        public Builder hasBeenAcceptedMap(@NonNull Map<String, Boolean> hasBeenAcceptedMap) {
            mHasBeenAccepted = hasBeenAcceptedMap;
            return this;
        }

        public Builder alwaysAccept(boolean alwaysAccept) {
            mAlwaysAccept = alwaysAccept;
            return this;
        }

        public Builder addFeature(String feature) {
            mFeatures.add(feature);
            return this;
        }

        /**
         * Sets the set of features for this account.
         *
         * @param features The set of account features.
         * @return This object, for chaining method calls.
         */
        public Builder featureSet(@NonNull Set<String> features) {
            mFeatures = features;
            return this;
        }

        public AccountHolder build() {
            if (mAccountId == null) {
                // CoreAccountId rejects strings containing '@' symbol.
                String fakeGaiaId = "gaia-id-" + mAccount.name.replace("@", "[at]");
                mAccountId = new CoreAccountId(fakeGaiaId);
            }
            return new AccountHolder(new CoreAccountInfo(mAccountId, mAccount), mAuthTokens,
                    mHasBeenAccepted, mAlwaysAccept, mFeatures);
        }
    }
}
