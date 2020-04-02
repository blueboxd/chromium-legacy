// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BROWSER_PROCESS_H_
#define WEBLAYER_BROWSER_BROWSER_PROCESS_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"

class PrefRegistrySimple;
class PrefService;

namespace network_time {
class NetworkTimeTracker;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace weblayer {

// Class that holds global state in the browser process. Should be used only on
// the UI thread.
class BrowserProcess {
 public:
  BrowserProcess();
  ~BrowserProcess();

  static BrowserProcess* GetInstance();

  // Does cleanup that needs to occur before threads are torn down.
  void StartTearDown();

  PrefService* GetLocalState();
  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory();
  network_time::NetworkTimeTracker* GetNetworkTimeTracker();

 private:
  void RegisterPrefs(PrefRegistrySimple* pref_registry);

  std::unique_ptr<PrefService> local_state_;
  std::unique_ptr<network_time::NetworkTimeTracker> network_time_tracker_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(BrowserProcess);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BROWSER_PROCESS_H_
