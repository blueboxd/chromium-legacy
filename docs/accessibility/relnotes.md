# Accessibility Release Notes

## TL;DR
Any change to accessibility-related files requires specifying an **AX-Relnotes**
field within the commit message, which describes the user-impacts the change
will have. Please see below for some examples of what this could look like:

* 'AX-Relnotes: ChromeVox now supports ...'
* 'AX-Relnotes: Fixed an issue where ... happened because of ...'
* 'AX-Relnotes: n/a', if the change has no user-facing effects.

If your change has user-impact, but is behind a feature flag, please use ‘n/a’.
At the time you remove the feature flag, the AX-Relnotes should mention that the
feature is being launched.

## What Are Release Notes
Every release cycle, the Chrome & Chrome OS Accessibility Team compiles release
notes for users, which detail any user-facing changes that have been made in
the upcoming release. These help our users stay informed about the features
and bugs our team works on, and are utilized by multiple testing teams.

## The Release Notes Process
Release notes are a collaborative effort and usually involve two or three team
members who own the process. The process is as follows:
1. One owner generates a list of commits for the release and pastes the output
in a shared Google Document.
2. The commits are split evenly among the owners.
3. Each owner parses their assigned commits, removing irrelevant information,
summarizing important content, contacting developers for clarification, and
categorizing commits with user-impact.
4. Once all commits have been categorized or deleted, one owner forwards the
release notes to the team's PgMs.
5. The PgMs take a final pass over the document, then forward it to relevant
stakeholders.

## Why AX-Relnotes
We require our developers to include an AX-Relnotes field in their commit
messages to make the release notes process more distributed and accurate.
Developers have the most context about how their changes affect users, and
having developers summarize their changes saves time and effort for the
release notes owners.

## Questions?
If you have any questions regarding accessibility release notes that have not
been answered in this document, please contact a member of
ui/accessibility/OWNERS.