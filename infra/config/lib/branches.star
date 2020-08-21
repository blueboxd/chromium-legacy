# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library containing utilities for providing branch-specific definitions.

The module provide the `branches` struct which provides access to versions of
a subset of luci functions with an additional `branch_selector` keyword argument
that controls what branches the definition is actually executed for. If
`branch_selector` doesn't match the current branch as determined by values on
the `settings` struct in '//project.star', then the resource is not defined. The
`branch_selector argument` can be one of the following constants:
* MAIN_ONLY - The resource is defined only for main/master/trunk
    [`settings.is_master`]
* STANDARD_RELEASES - The resource is defined for main/master/trunk and beta and
    stable branches
    [`settings.is_master and not settings.is_lts_branch`]
* ALL_RELEASES - The resource is defined for main/master/trunk, beta and stable
    branches and LTC/LTS branches
    [`True`]
The constants are also accessible via the `branches` struct.

For other uses cases where execution needs to vary by branch, the following are
also accessible via the `branches` struct:
* matches - Allows library code to be written that takes branch-specific
    behavior.
* value - Allows for providing different values between main/master/trunk and
    branches.
* exec - Allows for conditionally executing starlark modules.
"""

load("//project.star", "settings")

def _branch_selector(tag):
    return struct(__branch_selector__ = tag)

MAIN_ONLY = _branch_selector("MAIN_ONLY")
STANDARD_RELEASES = _branch_selector("STANDARD_RELEASES")
ALL_RELEASES = _branch_selector("ALL_RELEASES")

_BRANCH_SELECTORS = (MAIN_ONLY, STANDARD_RELEASES, ALL_RELEASES)

def _matches(branch_selector):
    """Returns whether `branch_selector` matches the project settings."""
    if branch_selector == MAIN_ONLY:
        return settings.is_master
    if branch_selector == STANDARD_RELEASES:
        return settings.is_master or not settings.is_lts_branch
    if branch_selector == ALL_RELEASES:
        return True
    fail("branch_selector must be one of {}, got {!r}".format(_BRANCH_SELECTORS, branch_selector))

def _value(*, for_main = None, for_branches = None):
    """Provide a value that varies between main/master/trunk and branches.

    If the current project settings indicate that this is main/master/trunk,
    then `for_main` will be returned. Otherwise, `for_branches` will be
    returned.
    """
    return for_main if settings.is_master else for_branches

def _exec(module, *, branch_selector = MAIN_ONLY):
    """Execute `module` if `branch_selector` matches the project settings."""
    if not _matches(branch_selector):
        return
    exec(module)

def _make_branch_conditional(fn):
    def conditional_fn(*args, branch_selector = MAIN_ONLY, **kwargs):
        if not _matches(branch_selector):
            return
        fn(*args, **kwargs)

    return conditional_fn

branches = struct(
    MAIN_ONLY = MAIN_ONLY,
    STANDARD_RELEASES = STANDARD_RELEASES,
    ALL_RELEASES = ALL_RELEASES,
    matches = _matches,
    exec = _exec,
    value = _value,

    # Make conditional versions of luci functions that define resources
    # This does not include any of the service configurations
    # This also does not include any functions such as recipe that don't
    # generate config unless they're referred to; it doesn't cause a problem
    # if they're not referred to
    **{a: _make_branch_conditional(getattr(luci, a)) for a in (
        "realm",
        "binding",
        "bucket",
        "builder",
        "gitiles_poller",
        "list_view",
        "list_view_entry",
        "console_view",
        "console_view_entry",
        "external_console_view",
        "cq_group",
        "cq_tryjob_verifier",
    )}
)
