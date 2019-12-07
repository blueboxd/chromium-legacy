// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/renderer/spelling_engine.h"

#include "build/build_config.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "services/service_manager/public/cpp/local_interface_provider.h"

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
#include "components/spellcheck/renderer/hunspell_engine.h"
#endif

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
#include "components/spellcheck/renderer/platform_spelling_engine.h"
#endif

SpellingEngine* CreateNativeSpellingEngine(
    service_manager::LocalInterfaceProvider* embedder_provider) {
  DCHECK(embedder_provider);
#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
#if defined(OS_WIN)
  if (spellcheck::UseWinHybridSpellChecker()) {
    // On Windows, when using the hybrid spellchecker, both Hunspell and the
    // platform are used for spellchecking. Ideally we'd want to know which
    // languages are supported by the platform spellchecker, and only create a
    // HunspellEngine for languages that aren't. Unfortunately, performing that
    // check must be done asynchronously on the browser side, while this
    // function must return synchronously. However, because the platform
    // spellchecker uses a code path that does not involve a SpellingEngine when
    // performing spellchecking, a solution is to create a HunspellEngine for
    // every language, even those that the platform supports. Because the
    // languages that the platform spellchecker supports are initialized on the
    // browser side, the HunspellEngine returned here will remain disabled for
    // these languages, so they will be skipped when performing the Hunspell
    // check.
    return new HunspellEngine(embedder_provider);
  } else if (spellcheck::UseBrowserSpellChecker()) {
    return new PlatformSpellingEngine(embedder_provider);
  }
#else
  return new PlatformSpellingEngine(embedder_provider);
#endif  // defined(OS_WIN)
#endif  // BUILDFLAG(USE_BROWSER_SPELLCHECKER)

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
  return new HunspellEngine(embedder_provider);
#endif  // BUILDFLAG(USE_RENDERER_SPELLCHECKER)
}
