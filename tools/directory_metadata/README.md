# Chromium METADATA files

METADATA.chromium files are a source-focused mechanism by which owners can
provide users of their code important information, including:

* The team responsible for the code.
* The Monorail component where bugs should be filed.
* The OS type.

METADATA.chromium files are structured protobuf files that are amenable to
programmatic interaction.

## Contents

This directory contains the
[proto definition](https://source.chromium.org/chromium/chromium/src/+/master:tools/directory_metadata/directory_metadata.proto)
for METADATA.chromium files, which is the source of truth about
METADATA.chromium file contents.

Historical information can be found in the
[original proposal](https://docs.google.com/document/d/17WMlceIMwge2ZiCvBWaBuk0w60YgieBd-ly3I8XsbzU/preview).

## Usage

METADATA.chromium files apply to all contents of a directory including its
subdirectories.

There is no inheritance mechanism, so any information in METADATA.chromium files
in parent directories is ignored.

For example, given the files below, the value of the `os` field for
a/b/METADATA.chromium would be `OS_UNSPECIFIED` regardless of the contents of
a/METADATA.chromium.

**a/METADATA.chromium**
```
monorail {
  project: "chromium"
  component: "Component"
}
team_email: "team@chromium.org"
os: OS_LINUX
```

**a/b/METADATA.chromium**
```
monorail {
  project: "chromium"
  component: "Component>Foo"
}
team_email: "foo-team@chromium.org"
```
