// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_ALTERNATIVE_BROWSER_DRIVER_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_ALTERNATIVE_BROWSER_DRIVER_H_

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/string_piece_forward.h"
#include "base/values.h"
#include "build/build_config.h"

class GURL;

namespace browser_switcher {

class BrowserSwitcherPrefs;

// Used for UMA metrics.
enum class BrowserType {
  kUnknown = 0,
  kIE = 1,
  kFirefox = 2,
  kOpera = 3,
  kSafari = 4,
  kChrome = 5,
  kMaxValue = kChrome,
};

// Interface for a helper class that does I/O operations for an
// |AlternativeBrowserLauncher|.
//
// - Reading from the Windows Registry
// - Communicating with an external process using the DDE protocol
// - Launching an external process
class AlternativeBrowserDriver {
 public:
  virtual ~AlternativeBrowserDriver();

  // Tries to launch |browser| at the specified URL, using whatever
  // method is most appropriate.
  virtual bool TryLaunch(const GURL& url) = 0;

  // Returns the localized string for the name of the alternative browser, if it
  // was auto-detected. If the name couldn't be auto-detected, returns an empty
  // string.
  virtual std::string GetBrowserName() const = 0;

  // Returns the type of browser as an enum, if it was auto-detected. Otherwise,
  // returns |BrowserType::kUnknown|.
  virtual BrowserType GetBrowserType() const = 0;
};

// Default concrete implementation for |AlternativeBrowserDriver|. Uses a
// platform-specific method to locate and launch the appropriate browser.
class AlternativeBrowserDriverImpl : public AlternativeBrowserDriver {
 public:
  explicit AlternativeBrowserDriverImpl(const BrowserSwitcherPrefs* prefs);
  ~AlternativeBrowserDriverImpl() override;

  // AlternativeBrowserDriver
  bool TryLaunch(const GURL& url) override;
  std::string GetBrowserName() const override;
  BrowserType GetBrowserType() const override;

  // Create the CommandLine object that would be used to launch an external
  // process.
  base::CommandLine CreateCommandLine(const GURL& url);

 private:
  using StringType = base::FilePath::StringType;

#if defined(OS_WIN)
  bool TryLaunchWithDde(const GURL& url);
  bool TryLaunchWithExec(const GURL& url);
#endif

  const BrowserSwitcherPrefs* const prefs_;

  DISALLOW_COPY_AND_ASSIGN(AlternativeBrowserDriverImpl);
};

}  // namespace browser_switcher

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_ALTERNATIVE_BROWSER_DRIVER_H_
