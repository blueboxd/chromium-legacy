// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// In support of roaming profiles on Windows, Chrome can be configured to store
// some profile data in a roaming profile location. Chrome accidentally created
// this roaming User Data directory during sync startup even when roaming
// profiles were not being used; see https://crbug.com/980487. This file here
// provides a mechanism to clean up empty User Data directories created in the
// default roaming profile location. This code should be deleted once the
// DeleteRoamingUserDataDirectory metric indicates that most of the affected
// population has been cleaned up.

#ifndef CHROME_BROWSER_SYNC_ROAMING_PROFILE_DIRECTORY_DELETER_WIN_H_
#define CHROME_BROWSER_SYNC_ROAMING_PROFILE_DIRECTORY_DELETER_WIN_H_

// Deletes an empty roaming User Data directory at some later time in the
// background.
void DeleteRoamingUserDataDirectoryLater();

#endif  // CHROME_BROWSER_SYNC_ROAMING_PROFILE_DIRECTORY_DELETER_WIN_H_
