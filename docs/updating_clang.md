# Updating clang

We distribute prebuilt packages of LLVM binaries, including clang and lld, that
all developers and bots pull at `gclient runhooks` time. These binaries are
just regular LLVM binaries built at a fixed upstream revision. This document
describes how to build a package at a newer revision and update Chromium to it.
An archive of all packages built so far is at https://is.gd/chromeclang

1.  Check that https://ci.chromium.org/p/chromium/g/chromium.clang/console
    looks reasonably green.
1.  Sync your Chromium tree to the latest revision to pick up any plugin
    changes
1.  Run `python tools/clang/scripts/upload_revision.py NNNN`
    with the target LLVM SVN revision number. This creates a roll CL on a new
    branch, uploads it and starts tryjobs that build the compiler binaries into
    a staging bucket on Google Cloud Storage (GCS).
1.  If the clang upload try bots succeed, copy the binaries from the staging
    bucket to the production one. For example:

    ```shell
    $ export rev=n123456-abcd1234-1
    $ for x in Linux_x64 Mac Win ; do \
        gsutil.py cp -n -a public-read gs://chromium-browser-clang-staging/$x/clang-$rev.tgz \
            gs://chromium-browser-clang/$x/clang-$rev.tgz ; \
        gsutil.py cp -n -a public-read gs://chromium-browser-clang-staging/$x/clang-$rev-buildlog.txt \
            gs://chromium-browser-clang/$x/clang-$rev-buildlog.txt ; \
        gsutil.py cp -n -a public-read gs://chromium-browser-clang-staging/$x/clang-tidy-$rev.tgz \
            gs://chromium-browser-clang/$x/clang-tidy-$rev.tgz ; \
        gsutil.py cp -n -a public-read gs://chromium-browser-clang-staging/$x/llvmobjdump-$rev.tgz \
            gs://chromium-browser-clang/$x/llvmobjdump-$rev.tgz ; \
        gsutil.py cp -n -a public-read gs://chromium-browser-clang-staging/$x/translation_unit-$rev.tgz \
            gs://chromium-browser-clang/$x/translation_unit-$rev.tgz ; \
        gsutil.py cp -n -a public-read gs://chromium-browser-clang-staging/$x/llvm-code-coverage-$rev.tgz \
            gs://chromium-browser-clang/$x/llvm-code-coverage-$rev.tgz ; \
        gsutil.py cp -n -a public-read gs://chromium-browser-clang-staging/$x/libclang-$rev.tgz \
            gs://chromium-browser-clang/$x/libclang-$rev.tgz ; \
        done && gsutil.py cp -n -a public-read gs://chromium-browser-clang-staging/Mac/lld-$rev.tgz \
            gs://chromium-browser-clang/Mac/lld-$rev.tgz
    ```

    **Note** that writing to this bucket requires special permissions. File a
    bug at g.co/bugatrooper if you don't have these already (e.g.,
    https://crbug.com/1034081).

1.  Run the goma package update script to push these packages to goma. If you do
    not have the necessary credentials to do the upload, ask clang@chromium.org
    to find someone who does
1.  Run an exhaustive set of try jobs to test the new compiler. The CL
    description created by upload_revision.py includes `Cq-Include-Trybots:`
    lines for all needed bots, so it's sufficient to just run `git cl try`
    (or hit "CQ DRY RUN" on gerrit).

1.  Commit roll CL from the first step
1.  The bots will now pull the prebuilt binary, and goma will have a matching
    binary, too.

## Adding files to the clang package

The clang package is downloaded unconditionally by all bots and devs. It's
called "clang" for historical reasons, but nowadays also contains other
mission-critical toolchain pieces besides clang.

We try to limit the contents of the clang package. They should meet these
criteria:

- things that are used by most developers use most of the time (e.g. a
  compiler, a linker, sanitizer runtimes)
- things needed for doing official builds

If you want to add something to the clang package that doesn't (yet?) meet
these criteria, you can make package.py upload it to a separate zip file
and then download it on an opt-in basis by using update.py's --package option.

If you're adding a new feature that you expect will meet the inclusion criteria
eventually but doesn't yet, start by having your things in a separate zip
and move it to the main zip once the criteria are met.
