# Render Tests

## Fixing a failing Render Test

Which section applies to the test you are investigating is determined by whether
the test class is manually creating a RenderTestRule or using the
SkiaGoldBuilder.

### Skia Gold Comparison

The newer form of pixel comparison backed by
[Skia Gold](https://skia.org/dev/testing/skiagold). If a test is running in
this mode, there will be mentions of "Skia Gold" in the reported failure.

#### Failing on trybots

Anytime a patchset produces new golden images, Gold should automatically
comment on your CL with a link to the triage page. If it fails to do so (e.g.
your test is failing because it is producing an image that was explicitly
marked as negative/invalid by someone previously), you can do the following to
access the triage page manually:

1. On the failed trybot run, locate and follow the `results_details` link under
the `chrome_public_test_apk` step to go to the **Suites Summary** page.
2. On the **Suites Summary** page, follow the link to the test suite that is
failing.
3. On the **Test Results of Suite** page, follow the links in the **log** column
corresponding to the renders mentioned in the failure stack trace. The links
will be named "Skia Gold triage link for entire CL".

Once on the triage page, make sure you are logged in at the top-right.
Currently, only @google.com accounts work, but other domains such as
chromium.org can be whitelisted if requested. You should then be able to
triage any newly produced images.

If the newly generated golden images are "breaking", i.e. it would be a
regression if Chrome continued to produce the old golden images (such as due
to a major UI change), you should **NOT** approve them as-is.

Instead:
1. Increment the revision using SkiaGoldBuilders's `setRevision`.
1. If you would like to add a description of what the revision represents that
will be visible on the Gold triage page, add or modify a call to
SkiaGoldBuilder's `setDescription`
1. Upload the new patchset, re-run the tryjobs, and approve the new baselines.

The revision increment is so that Gold separates any new golden images from any
that were produced before. It will affect **any** images produced using
RenderTestRule that had its revision incremented (i.e. any images produced in
that test class), so you may have to re-triage additional images. If there
are many images that need to be triaged, you can use the "Bulk Triage" option
in Gold under the "ACTIONS" menu item.

#### Failing on CI bots

If a test is failing on the CI bots, i.e. after a CL has already been merged,
you can perform the same steps as in the above section with the following
differences:

1. You must manually find the triage links, as Gold has nowhere to post a
comment to. Alternatively, you can check for untriaged images directly in the
[gold instance](https://chrome-gold.skia.org).
2. Triage links are for specific images instead of for an entire CL, and are
thus named after the the render name.

#### Failing locally

Skia Gold does not allow you to update golden images from local runs. You will
still have access to the generated image, the closest golden image, and the diff
between them in the test results, but this is purely for local debugging. New
golden images must come from either trybots or CI bots.

### Legacy/Local Pixel Comparison

The older form of pixel comparison that does everything locally. If a test is
running in this mode, there will be no mention of "Skia Gold" in the reported
failure.

#### Failing on trybots

To investigate why a Render Test is failing on the trybots:

1. On the failed trybot run, locate and follow the `results_details` link under
the `chrome_public_test_apk` step to go to the **Suites Summary** page.
2. On the **Suites Summary** page, follow the link to the test suite that is
failing.
3. On the **Test Results of Suite** page, follow the links in the **log** column
corresponding to the renders mentioned in the failure stack trace. The links
will be of the form `<test class>.<render id>.<device details>.png`.

Now you will see a **Render Results** page, showing:

* Some useful links.
* The **Failure** image, what the rendered Views look like on the test device.
* The **Golden** image, what the rendered Views should look like, according to
the golden files checked into the repository.
* A **Diff** image to help compare.

At this point, decide whether the UI change was intentional. If it was, follow
the steps below to update the golden files stored in the repository. If not, go
and fix your code! If there's some other error or flakiness, file a bug to
`peconn@chromium.org`.

1. Use the `Link to Golden` link to determine where in the repository the golden
was stored.
2. Right click on the `Download Failure Image` link to save the failure image in
the appropriate place in your local repository.
3. Run the script
`//chrome/test/data/android/manage_render_test_goldens.py upload` to upload the
new goldens to Google Storage and update the hashes used to download them.
4. Reupload the CL and run it through the trybots again.

When putting a change up for review that changes goldens, please include links
to the results_details/Render Results pages that you grabbed the new goldens
from. This will help reviewers confirm that the changes to the goldens are
acceptable.

If you add a new device/SDK combination that you expect golden images for, be
sure to add it to `ALLOWED_DEVICE_SDK_COMBINATIONS` in
`//chrome/test/data/android/manage_render_test_goldens.py`, otherwise the
goldens for it will not be uploaded.

#### Failing locally

Follow the steps in [*Running the tests locally*](#running-the-tests-locally)
below to generate renders.

You can rename the renders as appropriate and move them to the correct place in
the repository, or you can open the locally generated **Render Results** pages
and follow steps 2-3 in the second part of the
[*Failing on trybots*](#failing-on-trybots) section.


## Writing a new Render Test

### Writing the test

To write a new test, start with the example in the javadoc for
[RenderTestRule](https://cs.chromium.org/chromium/src/ui/android/javatests/src/org/chromium/ui/test/util/RenderTestRule.java)
or [ChromeRenderTestRule](https://cs.chromium.org/chromium/src/chrome/test/android/javatests/src/org/chromium/chrome/test/util/ChromeRenderTestRule.java).

To enable use of Skia Gold for managing golden images, use
RenderTestRule.SkiaGoldBuilder instead of creating a
RenderTestRule manually. This will become the default eventually, but is still
going through the experimental stage. If you want maximum stability, prefer the
older approach for now. If you want an easier rebaselining process in exchange
for potentially running into some early growing pains, prefer the use of Skia
Gold.

Rebaselining the old way requires downloading all new goldens locally, running
a script to upload them to a Google Storage bucket, and committing the updated
SHA1 files. Rebaselining via Gold is done entirely through a web UI.

If you want to separate your baselines from the default `android-render-tests`
corpus in Gold, you can call `setCorpus()` on your
`SkiaGoldBuilder` instance before calling `build()`.

**Note:** Each instance/corpus/description combination results in needing to
create a new Gold session under the hood, which adds ~250 ms due to extra
initialization the first time that combination is used in a particular test
suite run. Instance and corpus are pretty constant, so the main culprit is the
description. This overhead can be kept low by using identical descriptions in
multiple test classes, including multiple test cases in a test class that has a
description, or avoiding descriptions altogether.

### Running the tests locally

When running instrumentation tests locally, pass the `--local-output` option to
the test runner to generate a folder in your output directory (in the example
`out/Debug`) looking like `TEST_RESULTS_2017_11_09T13_50_49` containing the
failed renders, eg:

```
./out/Debug/bin/run_chrome_public_test_apk -A Feature=RenderTest --local-output
```

The golden images should be downloaded as part of the `gclient sync` process,
but if there appear to be goldens missing that should be there, try running
`//chrome/test/data/android/manage_render_test_goldens.py download` to ensure
that the downloaded goldens are current for the git revision.

### Generating golden images locally

**Note that this section only applies to tests running in the legacy/local pixel
comparison mode**

New golden images may be downloaded from the trybots or retrieved locally. This
section elaborates how to do the latter.

You should always create your reference images on the same device type as the
one running the tests. This is because each device/API version may produce a
slightly different image, eg. due to different screen dimensions, DPI setting,
or styling used across OS versions. This is also why each golden image name
includes the device name and API version.

When running a test with no goldens on the correct device, your tests should
fail with an exception:

```
RenderTest Goldens missing for: <reference>. See RENDER_TESTS.md for how to fix this failure.
```

You will be able to find the images the device captured on the device's SD card.

```
adb -d shell ls /sdcard/chromium_tests_root/chrome/test/data/android/render_tests/failures
```

## Implementation Details

### Supported devices

How a View is rendered depends on both the device model and the version of
Android it is running. We only want to maintain golden files for model/SDK pairs
that occur on the trybots, otherwise the golden files will get out of date as
changes occur and render tests will either fail on the Testers with no warning,
or be useless.

Currently, `chrome_public_test_apk` is only run on Nexus 5s running Android
Lollipop, so that is the only model/sdk combination for which we store goldens.
There [is work](https://crbug.com/731759) to expand this to include Nexus 5Xs
running Marshmallow as well.

### Sanitizing Views

Certain features lead to flaky tests, for example any sort of animation we don't
take into account while writing the tests. To help deal with this, you can use
`ChromeRenderTestRule.sanitize` to modify the View hierarchy and remove some of the
more troublesome attributes (for example, it disables the blinking cursor in
`EditText`s).

