// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/sync_engine_switches.h"

namespace switches {

const base::Feature kSyncResetPollIntervalOnStart{
    "SyncResetPollIntervalOnStart", base::FEATURE_DISABLED_BY_DEFAULT};

// Whether encryption keys should be derived using scrypt when a new custom
// passphrase is set. If disabled, the old PBKDF2 key derivation method will be
// used instead. Note that disabling this feature does not disable deriving keys
// via scrypt when we receive a remote Nigori node that specifies it as the key
// derivation method.
const base::Feature kSyncUseScryptForNewCustomPassphrases{
    "SyncUseScryptForNewCustomPassphrases", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSyncSupportTrustedVaultPassphrase{
    "SyncSupportTrustedVaultPassphrase", base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled sync cycle ends by collecting contributions from all datatypes
// and having less than max_commit_batch_size() entries to commit. If disabled
// it ends when attempt to collect contributions returned no entries to commit.
// TODO(crbug.com/1022293): Remove this flag after M82 or so.
const base::Feature kSyncPreventCommitsBypassingNudgeDelay{
    "SyncPreventCommitsBypassingNudgeDelay", base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace switches
