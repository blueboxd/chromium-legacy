# Clang Sheriffing

Chromium bundles its own pre-built version of [Clang](clang.md). This is done so
that Chromium developers have access to the latest and greatest developer tools
provided by Clang and LLVM (ASan, CFI, coverage, etc). In order to [update the
compiler](updating_clang.md) (roll clang), it has to be tested so that we can be
confident that it works in the configurations that Chromium cares about.

Fortunately, Chromium happens to be a pretty decent stress test for a C++
compiler, so we maintain a [waterfall of
builders](https://ci.chromium.org/p/chromium/g/chromium.clang/console) that
continuously build fresh versions of Clang and use them to build and test
Chromium. "Clang sheriffing" is the process of monitoring that waterfall,
determining if any compile or test failures are due to an upstream compiler
change, filing bugs upstream, and often reverting bad changes in LLVM. This
document describes some of the processes and techniques for doing that.

[TOC]

## Is it the compiler?

Chromium does not always build and pass tests in all configurations that
everyone cares about. Some configurations simply take too long to build
(ThinLTO) or be tested (dbg) on the CQ before committing. And, some tests are
flaky. So, our console is often filled with red boxes, and the boxes don't
always need to be green to roll clang.

Oftentimes, if a bot is red with a test failure, it's not a bug in the compiler.
To check this, the easiest and best thing to do is to try to find a
corresponding builder that doesn't use ToT clang. For standard configurations,
start on the waterfall that corresponds to the OS of the red bot, and search
from there. If the failing bot is Google Chrome branded, go to the (Google
internal) [official builder
list](https://uberchromegw.corp.google.com/i/official.desktop.continuous/builders/)
and start searching from there.

If you are feeling charitable, you can try to see when the test failure was
introduced by looking at the history in the bot. One way to do this is to add
`?numbuilds=200` to the builder URL to see more history. If that isn't enough
history, you can manually binary search build numbers by editing the URL until
you find where the regression was introduced. If it's immediately clear what CL
introduced the regression (i.e.  caused tests to fail reliably in the official
build configuration), you can simply load the change in gerrit and revert it,
linking to the first failing build that implicates the change being reverted.

If the failure looks like a compiler bug, these are the common failures we see
and what to do about them:

1. compiler crash
1. compiler warning change
1. compiler error
1. miscompile
1. linker errors

## Compiler crash

This is probably the most common bug. The standard procedure is to do these
things:

1. Use `got_clang_revision` property from first red and last green build to find
   upstream regression range
1. File a crbug documenting the crash. Include the range, and any other bots
   displaying the same symptoms.
1. Collect the crash reproduction files and reproduce the crash locally. When
   clang crashes, it prints something like this:
   ```
   PLEASE ATTACH THE FOLLOWING FILES TO THE BUG REPORT:
   Preprocessed source(s) and associated run script(s) are located at:
   clang: note: diagnostic msg: C:\src\tmp\t-8f292b.cpp
   clang: note: diagnostic msg: C:\src\tmp\t-8f292b.sh
   ```
   If you re-run the shell script, it should reproduce the crash.
1. Identify the revision that introduced the crash. First, look at the commit
   messages in the LLVM revision range to see if one modifies the code near the
   point of the crash. If so, try reverting it locally, rebuild, and run the
   reproducer to see if the crash goes away.

   If that doesn't work, use `git bisect`. Use this as a template for the bisect
   run script:
   ```shell
   #!/bin/bash
   cd $(dirname $0)  # get into llvm build dir
   ninja -j900 clang || exit 125 # skip revisions that don't compile
   ./t-8f292b.sh   # exit 0 if good, 0 if bad
   ```
1. Reply to the commit on `llvm-commits` or on the code review on
   [Phabricator](https://reviews.llvm.org/) letting the author know that you
   believe they introduced a regression, and that you will revert the patch and
   provide a reduced reproducer.
1. Revert the upstream LLVM change once you are confident that you have the
   right culprit. Tell the patch author that you will provide a reproduction.
1. Start a reduction using [CReduce](https://embed.cs.utah.edu/creduce/using/).
   Follow the docs there for writing an interestingness test and use it to
   reduce the input to something that can be provided upstream. Send the reduced
   reproducer to the patch author.

## Compiler warning change

New Clang versions often find new bad code patterns to warn on. Chromium builds
with `-Werror`, so improvements to warnings often turn into build failures in
Chromium. Once you understand the code pattern Clang is complaining about, file
a bug to do either fix or silence the new warning.

If this is a completely new warning, disable it by adding `-Wno-NEW-WARNING` to
[this list of disabled
warnings](https://cs.chromium.org/chromium/src/build/config/compiler/BUILD.gn?l=1479)
if `llvm_force_head_revision` is true. Here is [an
example](https://chromium-review.googlesource.com/1251622). This will keep the
ToT bots green while you decide what to do.

Sometimes, behavior changes and a pre-existing warning changes to warn on new
code. In this case, fixing Chromium may be the easiest and quickest fix. If
there are many sites, you may consider changing clang to put the new diagnostic
into a new warning group so you can handle it as a new warning as described
above.

If the warning is high value, then eventually our team or other contributors
will end up fixing the crbug and there is nothing more to do.  If the warning
seems low value, pass that feedback along to the author of the new warning
upstream. It's unlikely that it should be on by default or enabled by `-Wall` if
users don't find it valuable. If the warning is particularly noisy and can't be
easily disabled without disabling other high value warnings, you should consider
reverting the change upstream and asking for more discussion.

## Compiler error

This rarely happens, but sometimes clang becomes more strict and no longer
accepts code that it previously did. The standard procedure for a new warning
may apply, but it's more likely that the upstream Clang change should be
reverted, if the C++ code in question in Chromium looks valid.

## Miscompile

Miscompiles tend to result in crashes, so if you see a test with the CRASHED
status, this is probably what you want to do.

1. Bisect object files to find the object with the code that changed.
1. Debug it with a traditional debugger

## Linker error

TODO: Describe object file bisection, identify obj with symbol that no longer
has the section.

## Tips and tricks

Finding what object files differ between two directories:

```
$ diff -u <(cd out.good && find . -name "*.o" -exec sha1sum {} \; | sort -k2) \
          <(cd out.bad  && find . -name "*.o" -exec sha1sum {} \; | sort -k2)
```

Or with cmp:

```
$ find good -name "*.o" -exec bash -c 'cmp -s $0 ${0/good/bad} || echo $0' {} \;
```
