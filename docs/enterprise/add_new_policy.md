# Policy Settings in Chrome

## Summary

Chrome exposes a different set of configurations to administrators. These
configurations are called policy and they give administrators more advanced
controls than the normal users. With different device management tools,
an administrator can deliver these polices to many users. Here is the
[help center article](https://support.google.com/chrome/a/answer/9037717?hl=en)
that talks about Chrome policy and its deployment.

## Do I need a policy

Usually you need a policy when

-   Launching a new feature. Create a policy so that the admin can disable or
    enable the feature for all users.

-   Deprecating an old feature. Create a policy to give enterprise users more
    time to migrate away from the feature.

## Adding a new policy

1.  Think carefully about the name and the desired semantics of the new policy:
    -   Choose a name that is consistent with the existing naming scheme. Prefer
        "XXXEnabled" over "EnableXXX" because the former is more glanceable and
        sorts better.
    -   Consider the foreseeable future and try to avoid conflicts with possible
        future extensions or use cases.
    -   Negative policies (*Disable*, *Disallow*) are verboten because setting
        something to "true" to disable it confuses people.
2.  Declare the policy in the
    [policy_templates.json](https://cs.chromium.org/chromium/src/components/policy/resources/policy_templates.json)
    -   This file contains meta-level descriptions of all policies and is used
        to generate code, policy templates (ADM/ADMX for Windows and the
        application manifest for Mac), as well as documentation. Please make
        sure you get the version and feature flags (such as dynamic_refresh and
        supported_on) right.
    -   Here are the most used attributes. Please note that, all attributes
        below other than `supported_on` do not change the code behavior.
        -   `supported_on`: It controls the platform and Chrome milestone the
            policy supports.
        -   `dynamic_refresh`: It tells the admin whether the policy value can
            be changed and take effect without re-launching Chrome.
        -   `per_profile`: It tells the admin whether different policy values
            can be assigned to different profiles.
        -   `can_be_recommended`: It tells the admin whether they can mark the
            policy as recommended and allow the user to override it in the UI,
            using a command line switch or an extension.
        -   `future`: It hides the policy from auto-generated templates and
            documentation. It's used when your policy needs multiple milestone
            development.
    - The complete list of attributes and their expected values can be found in
      the
      [policy_templates.json](https://cs.chromium.org/chromium/src/components/policy/resources/policy_templates.json).
    -   The textual policy description should include the following:
        -   What features of Chrome are affected.
        -   Which behavior and/or UI/UX changes the policy triggers.
        -   How the policy behaves if it's left unset or set to invalid/default
            values. This may seem obvious to you, and it probably is. However,
            this information seems to be provided for Windows Group Policy
            traditionally, and we've seen requests from organizations to
            explicitly spell out the behavior for all possible values and for
            when the policy is unset.
3.  Create a preference and map the policy value to it.
    -   All policy values need to be mapped into a prefs value before being used
        unless the policy is needed before PrefService initialization.
    -   To map the policy:
        1.  Create a prefs and register the prefs in **Local State** or
            **Profile Prefs**. Please note that, this must match the
            `per_profile` attribute in the `policy_templates.json`. We also
            strongly encourage developers to register the prefs with **Profile
            Prefs** if possible, because this gives admin more flexibility of
            policy setup.
        2.  Most policies can be mapped to prefs with `kSimplePolicyMap` in
            [configuration_policy_handler_list_factory.cc](https://cs.chromium.org/chromium/src/chrome/browser/policy/configuration_policy_handler_list_factory.cc?type=cs&q=kSimplePolicyMap&g=0&l=150).
            If the policy needs additional verification or processing, please
            implement a `ConfigurationPolicyHandler` to do so.
        3.  Test the mapping by adding policy to
            [policy_test_cases.json](https://cs.chromium.org/chromium/src/chrome/test/data/policy/policy_test_cases.json?q=policy_test_case)
4.  Disable the user setting UI when the policy is applied.
    -   If your feature can be controlled by GUI in `chrome://settings`, the
        associated option should be disabled when the policy controlling it is
        managed.
        -   `PrefService:Preference::IsManaged` reveals whether a prefs value
            comes from policy or not.
        -   The setting needs an
            [indicator](https://cs.chromium.org/chromium/src/ui/webui/resources/images/business.svg)
            to tell users that the setting is enforced by the administrator.
5.  Support `dynamic_refresh` if possible.
    -   We strongly encourage developers to make their policies support this
        attribute. It means the admin can change the policy value and Chrome
        will honor the change at run-time without requiring a restart of the
        browser. Chrome OS does not always support non-dynamic profile policies.
        Please verify with a Chrome OS policy owner if your profile policy does
        not support dynamic refresh on Chrome OS.
    -   Most of the time, this requires a
        [PrefChangeRegistrar](https://cs.chromium.org/chromium/src/components/prefs/pref_change_registrar.h)
        to listen to the preference change notification and update UI or
        browser behavior right away.
6.  Adding a device policy for Chrome OS.
    -   Most policies that are used by the browser can be shared between desktop
        and Chrome OS. However, you need a few additional steps for a device
        policy on Chrome OS.
        -   Add a message for your policy in
            `components/policy/proto/chrome_device_policy.proto`. Please note
            that all proto fields are optional.
        -   Update
            `chrome/browser/chromeos/policy/device_policy_decoder_chromeos.{h,cc}`
            for the new policy.
7.  Test the policy.
    -   Add a test to verify the policy. You can add a test in
        `chrome/browser/policy/<area>_policy_browsertest.cc` or with the policy
        implementation. For example, a network policy test can be put into
        `chrome/browser/net`. Ideally, your test would set the policy, fire up
        the browser, and interact with the browser just as a user would do to
        check whether the policy takes effect.
8.  Manually testing your policy.
    -   Windows: The simplest way to test is to write the registry keys manually
        to `Software\Policies\Chromium` (for Chromium builds) or
        `Software\Policies\Google\Chrome` (for Google Chrome branded builds). If
        you want to test policy refresh, you need to use group policy tools and
        gpupdate; see
        [Windows Quick Start](https://www.chromium.org/administrators/windows-quick-start).
    -   Mac: See
        [Mac Quick Start](https://www.chromium.org/administrators/mac-quick-start)
        (section "Debugging")
    -   Linux: See
        [Linux Quick Start](https://www.chromium.org/administrators/linux-quick-start)
        (section "Set Up Policies")
    -   Chrome OS and Android are more complex to test, as a full end-to-end
        test requires network transactions to the policy test server.
        Instructions on how to set up the policy test server and have the
        browser talk to it are here:
        [Running the cloud policy test server](https://www.chromium.org/developers/how-tos/enterprise/running-the-cloud-policy-test-server).
        If you'd just like to do a quick test for Chrome OS, the Linux code is
        also functional on CrOS, see
        [Linux Quick Start](https://www.chromium.org/administrators/linux-quick-start).
9.  If you are adding a new policy that supersedes an older one, verify that the
    new policy works as expected even if the old policy is set (allowing us to
    set both during the transition time when Chrome versions honoring the old
    and the new policies coexist).
10. If your policy has interactions with other policies, make sure to document,
    test and cover these by automated tests.

## Examples

Here is an example based on the instructions above. It's a good, simple place to
get started:
[https://chromium-review.googlesource.com/c/chromium/src/+/1742209](https://chromium-review.googlesource.com/c/chromium/src/+/1742209)


## Modifying existing policies

If you are planning to modify an existing policy, please send out a one-pager to
client- and server-side stakeholders explaining the planned change.

There are a few noteworthy pitfalls that you should be aware of when updating
the code that handles existing policy settings, in particular:

- Make sure the policy metadata is up-to-date, in particular `supported_on`, and
the feature flags.
- In general, don't change policy semantics in a way that is incompatible
(as determined by user/admin-visible behavior) with previous semantics. **In
particular, consider that existing policy deployments may affect both old and
new browser versions, and both should behave according to the admin's
intentions**.
- **An important pitfall is that adding an additional allowed
value to an enum policy may cause compatibility issues.** Specifically, an
administrator may use the new policy value, which makes older Chrome versions
that may still be deployed (and don't understand the new value) fall back to
the default behavior. Carefully consider if this is OK in your case. Usually,
it is preferred to create a new policy with the additional value and deprecate
the old one.
- Don't rely on the cloud policy server for policy migrations because
this has been proven to be error prone. To the extent possible, all
compatibility and migration code should be contained in the client.
- It is OK to expand semantics of policy values as long as the previous policy
description is compatible with the new behavior (see the "extending enum"
pitfall above however).
- It is OK to update feature implementations and the policy
description when Chrome changes as long as the intended effect of the policy
remains intact.
- The process for removing policies is to deprecate them first,
wait a few releases (if possible) and then drop support for them. Make sure you
put the deprecated flag if you deprecate a policy.

### Presubmit Checks when Modifying Existing Policies

To enforce the above rules concerning policy modification and ensure no
backwards incompatible changes are introduced, presubmit checks
will be performed on every change to
[policy_templates.json](https://cs.chromium.org/chromium/src/components/policy/resources/policy_templates.json).

The presubmit checks perform the following verifications:

1.  It verifies whether a policy is considered **unreleased** before allowing a
    change. A policy is considered unreleased if **any** of the following
    conditions are true:

    1.  It is an unchanged policy marked as `future: true`.
    2.  All the `supported_versions` of the policy satisfy **any** of the
        following conditions
        1.  The unchanged supported major version is >= the current major
            version stored in the VERSION file at tip of tree. This covers the
            case of a policy that has just recently been added, but has not yet
            been released to a stable branch.
        2.  The changed supported version == unchanged supported version + 1 and
            the changed supported version is equal to the version stored in the
            VERSION file at tip of tree. This check covers the case of
            "unreleasing" a policy after a new stable branch has been cut, but
            before a new stable release has rolled out. Normally such a change
            should eventually be merged into the stable branch before the
            release.

2.  If the policy is considered **unreleased**, all changes to it are allowed.

3.  However if the policy is released then the following verifications
    are performed on the delta between the original policy and the changed
    policy.

    1.  Released policies cannot be removed.
    2.  Released policies cannot have their type changed (e.g. from bool to
        Enum).
    3.  Released policies cannot have the `future: true` flag added to it. This
        flag can only be set on a new policy.
    4.  Released policies can only add additional `supported_on` versions. They
        cannot remove or modify existing values for this field except for the
        special case above for determining if a policy is released. Policy
        support end version (adding "-xx" ) can however be added to the
        supported_on version to specify that a policy will no longer be
        supported going forward (as long as the initial `supported_on` version
        is not changed).
    5.  Released policies cannot be renamed (this is the equivalent of a
        delete + add).
    6.  Released policies cannot change their `device_only` flag. This flag can
        only be set on a new policy.
    7.  Released policies with non dict types cannot have their schema changed.
        1.  For enum types this means values cannot be renamed or removed (these
            should be marked as deprecated instead).
        2.  For int types, we will allow making the minimum and maximum values
            less restrictive than the existing values.
        3.  For string types, we will allow the removal of the "pattern"
            property to allow the validation to be less restrictive.
        4.  We will allow addition to any list type values only at the end of
            the list of values and not in the middle or at the beginning (this
            restriction will cover the list of valid enum values as well).
        5.  These same restrictions will apply recursively to all property
            schema definitions listed in a dictionary type policy.
    8.  Released dict policies cannot remove or modify any existing keys in
        their schema. They can only add new keys to the schema.
        1.  Dictionary policies can have some of their "required" fields removed
            in order to be less restrictive.

## Cloud Policy

**For Googlers only**: The Cloud Policy will be maintained by the Admin console
team.
[See instructions here](https://docs.google.com/document/d/1QgDTWISgOE8DVwQSSz8x5oKrI3O_qAvOmPryE5DQPcw/edit?usp=sharing)
on how to update the Cloud Policy.

## Post policy update

Once the policy is added or modified, nothing else needs to be taken
care of by the Chromium developers. However, there are a few things that will be
updated based on the json file. Please note that there is no ETA for
everything listed below.

* [Policy templates](https://dl.google.com/dl/edgedl/chrome/policy/policy_templates.zip)
  will be updated automatically.
* [Policy documentation](https://cloud.google.com/docs/chrome-enterprise/policies/)
  will be updated automatically.

------

### Additional Notes

1. policy_templates.json is actually a Python dictionary even though the file name contains *json*.
