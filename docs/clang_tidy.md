# Clang Tidy

[TOC]

## Introduction

[clang-tidy](http://clang.llvm.org/extra/clang-tidy/) is a clang-based C++
“linter” tool. Its purpose is to provide an extensible framework for diagnosing
and fixing typical programming errors, like style violations, interface misuse,
or bugs that can be deduced via static analysis.

## Where is it?

clang-tidy is available in two places in Chromium:

- In Chromium checkouts
- In code review on Gerrit

Clang-tidy automatically runs on any CL that Chromium committers upload to
Gerrit, and will leave code review comments there. This is the recommended way
of using clang-tidy.

## Enabled checks

Chromium globally enables a subset of all of clang-tidy's checks (see
`${chromium}/src/.clang-tidy`). We want these checks to cover as much as we
reasonably can, but we also strive to strike a reasonable balance between signal
and noise on code reviews. Hence, a large number of clang-tidy checks are
disabled.

### Adding a new check

If you'd like to propose the addition of a new check, please send an email to
cxx@chromium.org describing why you think the check is helpful. If the proposed
check is approved, you may turn it on, though please note that this is only
provisional approval: we get signal from users clicking "Not Useful" on
comments. If feedback is overwhelmingly "users don't find this useful," the
check may be removed.

## But I want to run it locally

If you want to sync the officially-supported `clang-tidy` to your workstation,
add the following to your .gclient file:

```
solutions = [
  {
    'custom_vars': {
      'checkout_clang_tidy': True,
    },
  },
]
```

If you already have `solutions` and `custom_vars`, just add
`checkout_clang_tidy` to the existing `custom_vars` map.

Once the above update has been made, run `gclient runhooks`, and clang-tidy
should appear at `src/third_party/llvm-build/Release+Asserts/bin/clang-tidy` if
your Chromium tree is sufficiently up-to-date.

### Running clang-tidy locally

**Note** that the local flows with clang-tidy are experimental, and require an
LLVM checkout. Tricium is happy to run on WIP CLs, and we strongly encourage its
use.

That said, assuming you have the LLVM sources available, you'll need to bring
your own `clang-apply-replacements` binary if you want to use the `-fix` option
noted below.

Running clang-tidy is (hopefully) simple.
1.  Build chrome normally.
```
ninja -C out/Release chrome
```
2.  Enter the build directory
```
cd out/Release
```
3.  Export Chrome's compile command database
```
gn gen . --export-compile-commands
```
4.  Run clang-tidy.
```
<PATH_TO_LLVM_SRC>/clang-tidy/tool/run-clang-tidy.py \
    -p . \# Set the root project directory, where compile_commands.json is.
    # Set the clang-tidy binary path, if it's not in your $PATH.
    -clang-tidy-binary <PATH_TO_LLVM_BUILD>/bin/clang-tidy \
    # Set the clang-apply-replacements binary path, if it's not in your $PATH
    # and you are using the `fix` behavior of clang-tidy.
    -clang-apply-replacements-binary \
        <PATH_TO_LLVM_BUILD>/bin/clang-apply-replacements \
    # The checks to employ in the build. Use `-*` to omit default checks.
    -checks=<CHECKS> \
    -header-filter=<FILTER> \# Optional, limit results to only certain files.
    -fix \# Optional, used if you want to have clang-tidy auto-fix errors.
    chrome/browser # The path to the files you want to check.

Copy-Paste Friendly (though you'll still need to stub in the variables):
<PATH_TO_LLVM_SRC>/clang-tools-extra/clang-tidy/tool/run-clang-tidy.py \
    -p . \
    -clang-tidy-binary <PATH_TO_LLVM_BUILD>/bin/clang-tidy \
    -clang-apply-replacements-binary \
        <PATH_TO_LLVM_BUILD>/bin/clang-apply-replacements \
    -checks=<CHECKS> \
    -header-filter=<FILTER> \
    -fix \
    chrome/browser
```

\*It's not clear which, if any, `gn` flags may cause issues for
`clang-tidy`. I've had no problems building a component release build,
both with and without goma. if you run into issues, let us know!

### Questions

Questions about the local flow? Reach out to rdevlin.cronin@chromium.org,
thakis@chromium.org, or gbiv@chromium.org.

Questions about the Gerrit flow? Email tricium-dev@google.com or
infra-dev+tricium@chromium.org, or file a bug against `Infra>Platform>Tricium`.
Please CC gbiv@chromium.org on any of these.

Discoveries? Update the doc!
