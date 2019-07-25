# GPU Pixel Testing With Gold

This page describes in detail the GPU pixel tests that utilize Skia Gold for
storing and comparing images (the `pixel_skia_gold_test` type). This includes
information such as how Skia Gold works, how to triage images produced by these
tests, and working on Gold itself.

[TOC]

## Skia Gold

[Gold][gold documentation] is an image diff service developed by the Skia team.
It was originally developed solely for Skia's usage and only supported
post-submit tests, but has been picked up by other projects such as Chromium and
PDFium and now supports trybots. Unlike other image diff solutions in Chromium,
comparisons are done in an external service instead of locally on the testing
machine.

[gold documentation]: https://skia.org/dev/testing/skiagold

### Why Gold

Gold has three main advantages over the traditional local image comparison
historically used by Chromium:

1. Triage time can be much lower. Because triaging is handled by an external
service, new golden images don't need to go through the CQ and wait for
waterfall bots to pick up the CL. Once an image is triaged in Gold, it
becomes immediately available for future test runs.
2. Gold supports multiple approved images per test. It is not uncommon for
tests to produce images that are visually indistinguishable, but differ in
a handful of pixels by a small RGB value. Fuzzy image diffing can solve this
problem, but introduces its own set of issues such as possibly causing a test
to erroneously pass. Since most tests that exhibit this behavior only actually
produce 2 or 3 possible valid images, being able to say that any of those
images are acceptable is simpler and less error-prone.
3. Better image storage. Traditionally, images had to either be included
directly in the repository or uploaded to a Google Storage bucket and pulled in
using the image's hash. The former allowed users to easily see which images were
currently approved, but storing large sized or numerous binary files in git is
generally discouraged due to the way git's history works. The latter worked
around the git issues, but made it much more difficult to actually see what was
being used since the only thing the user had to go on was a hash. Gold moves the
images out of the repository, but provides a GUI interface for easily seeing
which images are currently approved for a particular test.

### How It Works

Gold consists of two main parts: the Gold instance/service and the `goldctl`
binary. A Gold instance in turn consists of two parts: a Google Storage bucket
that data is uploaded to and a server running on GCE that ingests the data and
provides a way to triage diffs. `goldctl` simply provides a standardized way
of interacting with Gold - uploading data to the correct place, retrieving
baselines/golden information, etc.

In general, the following order of events occurs when running a Gold-enabled
test:

1. The test produces an image and passes it to `goldctl`, along with some
information about the hardware and software configuration that the image was
produced on, the test name, etc.
2. `goldctl` checks whether the hash of the produced image is in the list of
approved hashes.
    1. If it is, `goldctl` exits with a non-failing return code and nothing else
    happens. At this point, the test is finished.
    2. If it is not, `goldctl` uploads the image and metadata to the storage
    bucket and exits with a failing return code.
3. The server sees the new data in the bucket and ingests it, showing a new
untriaged image in the GUI.
4. A user approves the new image in the GUI, and the server adds the image's
hash to the baselines. See the [Waterfall Bots](#Waterfall-Bots) and
[Trybots](#Trybots) sections for specifics on this.
5. The next time the test is run, the new image is in the baselines, and
assuming the test produces the same image again, the test passes.

While this is the general order of events, there are several differences between
waterfall/CI bots and trybots.

#### Waterfall Bots

Waterfall bots are the simpler of the two bot types. There is only a single
set of baselines to worry about, which is whatever baselines were approved for
a git revision. Additionally, any new images that are produced on waterfalls are
all lumped into the same group of "untriaged images on master", and any images
that are approved from here will immediately be added to the set of baselines
for master.

Since not all waterfall bots have a trybot counterpart that can be relied upon
to catch newly produced images before a CL is committed, it is likely that a
change that produces new goldens on the CQ will end up making some of the
waterfall bots red for a bit, particularly those on chromium.gpu.fyi. They will
remain red until the new images are triaged as positive or the tests stop
producing the untriaged images. So, it is best to keep an eye out for a few
hours after your CL is committed for any new images from the waterfall bots that
need triaging.

#### Trybots

Trybots are a little more complicated when it comes to retrieving and approving
images. First, the set of baselines that are provided when requested by a test
is the union of the master baselines for the current revision and any baselines
that are unique to the CL. For example, if an image with the hash `abcd` is in
the master baselines for `FooTest` and the CL being tested has also approved
an image with the hash `abef` for `FooTest`, then the provided baselines will
contain both `abcd` and `abef` for `FooTest`.

When an image associated with a CL is approved, the approval only applies to
that CL until the CL is merged. Once this happens, any baselines produced by the
CL are automatically merged into the master baselines for whatever git revision
the CL was merged as. In the above example, if the CL was merged as commit
`ffff`, then both `abcd` and `abef` would be approved images on master from
`ffff` onward.

## Triaging Failures

### Standard Workflow

The current approach to getting the triage links for new images is:

1. Navigate to the failed build.
2. Search for the failed step for the `pixel_skia_gold_test` test.
3. Look for either:
    1. One or more links named `gold_triage_link for <test name>`. This will be
    the case if there are fewer than 10 links.
    2. A single link named
    `Too many artifacts produced to link individually, click for links`. This
    will be the case if there are 10 or more links.
4. In either case, follow the link(s) to the triage page for the image the
failing test produced.
5. Triage images on those pages (typically by approving them, but you can also
mark them as a negative if it is an image that should not be produced). In the
case of a negative image, a bug should be filed on [crbug] to investigate and
fix the cause of the that particular image being produced, as future
occurrences of it will cause the test to fail. Such bugs should include the
`Internals>GPU>Testing` component and whatever component is suitable for the
type of failing test (likely `Blink>WebGL` or `Blink>Canvas`). The test should
also be marked as failing or skipped in
`//content/test/gpu/gpu_tests/test_expectations/pixel_expectations.txt` so that
the test failure doesn't show up as a builder failure. If the failure is
consistent, prefer to skip instead of mark as failing so that the failure links
don't pile up. If the failure occurs on the trybots, include the change to the
expectations in your CL.

[crbug]: https://crbug.com

In the future, a comment will also be automatically posted to CLs that produce
new images with a link to triage them.

In addition, you can see all currently untriaged images that are currently
being produced on ToT on the [GPU Gold instance's main page][gpu gold instance]
and currently untriaged images for a CL by substituting the Gerrit CL number
into
`https://chrome-gpu-gold.skia.org/search?issue=[CL Number]&unt=true&master=true`.

[gpu gold instance]: https://chrome-gpu-gold.skia.org

It's possible, particularly if a test is regularly producing multiple images,
for an image to be untriaged but not show up on the front page of the Gold
instance (for details, see [this crbug comment][untriaged non tot comment]). To
see all such images, visit [this link][untriaged non tot].

[untriaged non tot comment]: https://bugs.chromium.org/p/skia/issues/detail?id=9189#c4
[untriaged non tot]: https://chrome-gpu-gold.skia.org/search?fdiffmax=-1&fref=false&frgbamax=255&frgbamin=0&head=false&include=false&limit=50&master=false&match=name&metric=combined&neg=false&offset=0&pos=false&query=source_type%3Dchrome-gpu&sort=desc&unt=true

### Finding A Failed Build

If for some reason you know that a test run produced a bad image, but do not
have a direct link to the failed build (e.g. you found a bad image using the
untriaged non-ToT link from above), you may want to find the failed Swarming
task to help debug the issue. Gold currently provides a list of CLs that were
under test when a particular image was produced, but does not provide a link to
the build that produced it, so the following workaround can be used.

Assuming the failure is relatively recent (within the past week or so), you can
use the flakiness dashboard to help find the failed run. To do so, substitute
the test name into
`https://test-results.appspot.com/dashboards/flakiness_dashboard.html#showAllRuns=true&testType=pixel_skia_gold_test&tests=[test_name]`
and scroll through the history until you find the failed build (represented by
a red square). Click on the build and follow the `Build log` link. This will
take you to the failed build, from which you can get to the Swarming task like
normal by scrolling to the failed step and clicking on the link for the failed
shard number.

### Triaging A Specific Image

If for some reason an image is not showing up in Gold but you know the hash, you
can manually navigate to the page for it by filling in the correct information
to `https://chrome-gpu-gold.skia.org/detail?test=[test_name]&digest=[hash]`.
From there, you should be able to triage it as normal.

If this happens, please also file a bug in [Skia's bug tracker][skia crbug] so
that the root cause can be investigated and fixed. It's likely that you will
be unable to directly edit the owner, CC list, etc. directly, in which case
ping kjlubick@ with a link to the filed bug to help speed up triaging. Include
as much detail as possible, such as a links to the failed swarming task and
the triage link for the problematic image.

[skia crbug]: https://bugs.chromium.org/p/skia

## Running Locally

Normally, `goldctl` uploads images and image metadata to the Gold server when
used. This is not desirable when running locally for a couple reasons:

1. Uploading requires the user to be whitelisted on the server, and whitelisting
everyone who wants to run the tests locally is not a viable solution.
2. Images produced during local runs are usually slightly different from those
that are produced on the bots due to hardware/software differences. Thus, most
images uploaded to Gold from local runs would likely only ever actually be used
by tests run on the machine that initially generated those images, which just
adds noise to the list of approved images.

Additionally, the tests normally rely on the Gold server for viewing images
produced by a test run. This does not work if the data is not actually uploaded.

In order to get around both of these issues, simply pass the `--local-run` flag
to the tests. This will disable uploading, but otherwise go through the same
steps as a test normally would. Each test will also print out a `file://` URL to
the image it produces and a link to all approved images for that test in Gold.

Because the image produced by the test locally is likely slightly different from
any of the approved images in Gold, local test runs are likely to fail during
the comparison step. In order to cut down on the amount of noise, you can also
pass the `--no-skia-gold-failure` flag to not fail the test on a failed image
comparison. When using `--no-skia-gold-failure`, you'll also need to pass the
`--passthrough` flag in order to actually see the link output.

Example usage:
`run_gpu_integration_test.py pixel --no-skia-gold-failure --local-run
--passthrough --build-revision aabbccdd --test-machine-name local`

## Working On Gold

### Modifying Gold And goldctl

Although uncommon, changes to the Gold service and `goldctl` binary may be
needed. To do so, simply get a checkout of the
[Skia infrastructure repo][skia infra repo] and go through the same steps as
a Chromium CL (`git cl upload`, etc.).

[skia infra repo]: https://skia.googlesource.com/buildbot/

The Gold service code is located in the `//golden/` directory, while `goldctl`
is  located in `//gold-client/`. Once your change is merged, you will have to
either contact kjlubick@google.com to roll the service version or follow the
steps in [Rolling goldctl](#Rolling-goldctl) to roll the `goldctl` version used
by Chromium.

### Rolling goldctl

`goldctl` is available as a CIPD package and is DEPSed in as part of `gclient
sync` To update the binary used in Chromium, perform the following steps:

1. (One-time only) get an [infra checkout][infra repo]
2. Run `infra $ ./go/env.py` and run each of the commands it outputs to change
your GOPATH and other environment variables for your terminal
3. Update the Skia revision in [`deps.yaml`][deps yaml] to match the revision
of your `goldctl` CL (or some revision after it)
4. Run `infra $ ./go/deps.py update` and copy the list of repos it updated
(include this in your CL description)
5. Upload the changelist ([sample CL][sample roll cl])
6. Once the CL is merged, wait until the git revision of your merged CL shows
up in the tags of one of the instances for the [CIPD package][goldctl package]
7. Update the [revision in DEPS][goldctl deps entry] to be the merged CL's
revision

[infra repo]: https://chromium.googlesource.com/infra/infra/
[deps yaml]: https://chromium.googlesource.com/infra/infra/+/91333d832a4d871b4219580dfb874b49a97e6da4/go/deps.yaml#432
[sample roll cl]: https://chromium-review.googlesource.com/c/infra/infra/+/1493426
[goldctl package]: https://chrome-infra-packages.appspot.com/p/skia/tools/goldctl/linux-amd64/+/
[goldctl deps entry]: https://chromium.googlesource.com/chromium/src/+/6b7213a45382f01ac0a2efec1015545bd051da89/DEPS#1304

If you want to make sure that `goldctl` builds after the update before
committing (e.g. to ensure that no extra third party dependencies were added),
run the following after the `./go/deps.py update` step:

1. `infra $ ./go/deps.py install`
2. `infra $ go install go.skia.org/infra/gold-client/cmd/goldctl`
3. `infra $ which goldctl` which should point to a binary in `infra/go/bin/`
4. `infra $ goldctl` to make sure it actually runs
