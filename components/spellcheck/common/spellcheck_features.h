// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_FEATURES_H_
#define COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/spellcheck/spellcheck_buildflags.h"

namespace spellcheck {

#if BUILDFLAG(ENABLE_SPELLCHECK)

#if BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
extern const base::Feature kWinUseHybridSpellChecker;
#endif  // BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)

bool UseBrowserSpellChecker();

#if defined(OS_WIN)
extern const base::Feature kWinUseBrowserSpellChecker;

// If enabled, don't initialize the SpellcheckService on browser launch.
// Since Chromium already by default initializes the spellcheck service
// on startup for custom dictionary synchronization, the command line
// for launching the browser with Windows hybrid spellchecking enabled
// but no initialization of the spellcheck service is:
//    chrome
//    --enable-features=WinUseBrowserSpellChecker,WinDelaySpellcheckServiceInit
//    --disable-sync-types="Dictionary"
extern const base::Feature kWinDelaySpellcheckServiceInit;

bool WindowsVersionSupportsSpellchecker();
bool UseWinHybridSpellChecker();
#endif  // defined(OS_WIN)

#if defined(OS_ANDROID)
extern const base::Feature kAndroidSpellChecker;
extern const base::Feature kAndroidSpellCheckerNonLowEnd;

bool IsAndroidSpellCheckFeatureEnabled();
#endif  // defined(OS_ANDROID)

#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

}  // namespace spellcheck

#endif  // COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_FEATURES_H_
