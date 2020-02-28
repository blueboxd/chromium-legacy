# About the 'use_browser_spellchecker' and 'use_renderer_spellchecker' build time flag.

## use_browser_spellchecker
Use the operating system's spellchecker rather than Hunspell. This does
not affect the "red underline" spellchecker which can consult Google's
server-based spellcheck service.

## use_renderer_spellchecker
Use hunspell spellchecker rather than the operating system's spellchecker.

## Note on Windows:
For most operating system except Windows, the decision to use the platform
spellchecker or Hunspell spellchecker is made at build time. Therefore,
use_browser_spellchecker and use_renderer_spellchecker are mutually
exclusive for most operating systems except Windows.

For Windows OS, the decision to use the platform spellchecker or Hunspell
spellchecker is made during runtime. Therefore, we include both build
flags if the platform is Windows.

We also need to create the runtime feature flag kWinUseBrowserSpellChecker
for Windows OS. The feature flag is used to choose between the platform or
Hunspell spellchecker at runtime.

## Note on the Windows Hybrid spellchecker
On Windows, the platform spell checker relies on user-installed language packs.
It is possible for a user to enable spell checking in Chrome for a language
while not having installed the necessary language pack in the Windows settings.

For those situations, the spell checker on Windows will first try to use the
platform spell checker. For languages where this is not possible, Hunspell will
be used instead.

To control this fallback logic, a separate build flag
use_win_hybrid_spellchecker is used. In addition, for experiment purposes, a
feature flag kWinUseHybridSpellChecker is used to control this behavior at
runtime. Finally, this fallback logic requires the platform spell checker to be
available, so it also uses the browser spell checker checks described above.
