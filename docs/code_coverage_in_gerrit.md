# Code Coverage in Gerrit

Tests are critical because they find bugs and regressions, enforce better
designs and make code easier to maintain. **Code coverage helps you ensure your
tests are thorough**.

Chromium CLs can show a line-by-line breakdown of test coverage. **You can use
the code coverage trybot to ensure you only submit well-tested code**.

To see code coverage for a Chromium CL, trigger the code coverage trybot
**linux-coverage-rel**:

![choose_tryjobs] ![linux_coverage_rel]

Once the build finishes and code coverage data is processed successfully, **look
at the right column of the side by side diff view to see coverage information**:

![code_coverage_annotations]

The code coverage tool currently **supports C/C++ code for Chrome on Linux**;
support for more platforms and more languages is in progress.

The code coverage trybot has been **rolled out to a 10% experiment**, and once
we're more comfortable in its stability, we plan to enable it by default and
expand it to more platforms.

## Contacts

### Reporting problems
For any breakage report and feature requests, please [file a bug].

### Mailing list
For questions and general discussions, please join [code-coverage group].

## FAQ
### Why is coverage not shown even though the try job finished successfully?

There are several possible reasons:
* A particular source file/test may not be available on a particular project or
platform. As of now, only `chromium/src` project and `Linux` platform is
supported.
* There is a bug in the pipeline. Please [file a bug] to report the breakage.

### How does it work?

Please refer to [code_coverage.md] for how code coverage works in Chromium in
general, and specifically, for per-CL coverage in Gerrit, the
[clang_code_coverage_wrapper] is used to compile and instrument ONLY the source
files that are affected by the CL for the sake of performance and a
[chromium-coverage Gerrit plugin] is used to display code coverage information
in Gerrit.


[choose_tryjobs]: images/code_coverage_choose_tryjobs.png
[linux_coverage_rel]: images/code_coverage_linux_coverage_rel.png
[code_coverage_annotations]: images/code_coverage_annotations.png
[file a bug]: https://bugs.chromium.org/p/chromium/issues/entry?components=Tools%3ECodeCoverage
[code-coverage group]: https://groups.google.com/a/chromium.org/forum/#!forum/code-coverage
[code_coverage.md]: code_coverage.md
[clang_code_coverage_wrapper]: clang_code_coverage_wrapper.md
[chromium-coverage Gerrit plugin]: https://chromium.googlesource.com/infra/gerrit-plugins/code-coverage/
