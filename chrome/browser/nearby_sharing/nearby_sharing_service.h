// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_H_

// This service implements Nearby Sharing on top of the Nearby Connections mojo.
// Currently only single profile will be allowed to be bound at a time and only
// after the user has enabled Nearby Sharing in prefs.
class NearbySharingService {
 public:
  virtual ~NearbySharingService() = default;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_H_
