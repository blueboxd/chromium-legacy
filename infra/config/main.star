#!/usr/bin/env lucicfg
# See https://chromium.googlesource.com/infra/luci/luci-go/+/HEAD/lucicfg/doc/README.md
# for information on starlark/lucicfg

lucicfg.check_version(
    min = '1.13.1',
    message = 'Update depot_tools',
)

# Tell lucicfg what files it is allowed to touch
lucicfg.config(
    config_dir = 'generated',
    tracked_files = [
        'commit-queue.cfg',
        'cq-builders.md',
        'cr-buildbucket.cfg',
        'luci-logdog.cfg',
        'luci-milo.cfg',
        'luci-notify.cfg',
        'luci-scheduler.cfg',
        'project.cfg',
        'tricium-prod.cfg',
    ],
    fail_on_warnings = True,
)

# Just copy tricium-prod.cfg to the generated outputs
lucicfg.emit(
    dest = 'tricium-prod.cfg',
    data = io.read_file('tricium-prod.cfg'),
)

luci.project(
    name = 'chromium',
    buildbucket = 'cr-buildbucket.appspot.com',
    logdog = 'luci-logdog.appspot.com',
    milo = 'luci-milo.appspot.com',
    notify = 'luci-notify.appspot.com',
    scheduler = 'luci-scheduler.appspot.com',
    swarming = 'chromium-swarm.appspot.com',
    acls = [
        acl.entry(
            roles = [
                acl.LOGDOG_READER,
                acl.PROJECT_CONFIGS_READER,
                acl.SCHEDULER_READER,
            ],
            groups = 'all',
        ),
        acl.entry(
            roles = acl.LOGDOG_WRITER,
            groups = 'luci-logdog-chromium-writers',
        ),
        acl.entry(
            roles = acl.SCHEDULER_OWNER,
            groups = 'project-chromium-admins',
        ),
    ],
)

luci.cq(
    submit_max_burst = 2,
    submit_burst_delay = time.minute,
    status_host = 'chromium-cq-status.appspot.com',
)

# Declare a CQ group that watches all branch heads
# We won't add any builders, but SUBMIT TO CQ fails on Gerrit if there is no CQ
# group
luci.cq_group(
    name = 'fallback-empty-cq',
    retry_config = cq.RETRY_ALL_FAILURES,
    watch = cq.refset(
        repo = 'https://chromium.googlesource.com/chromium/src',
        refs = ['refs/branch-heads/.+'],
    ),
    acls = [
        acl.entry(
            acl.CQ_COMMITTER,
            groups = 'project-chromium-committers',
        ),
        acl.entry(
            acl.CQ_DRY_RUNNER,
            groups = 'project-chromium-tryjob-access',
        ),
    ],
)

luci.logdog(
    gs_bucket = 'chromium-luci-logdog',
)

luci.milo(
    logo = 'https://storage.googleapis.com/chrome-infra-public/logo/chromium.svg',
)

exec('//recipes.star')

exec('//notifiers.star')

exec('//subprojects/chromium/main.star')
exec('//subprojects/findit/main.star')
exec('//subprojects/goma/main.star')
exec('//subprojects/webrtc/main.star')

exec('//generators/cq-builders-md.star')
# This should be exec'ed before exec'ing scheduler-noop-jobs.star because
# attempting to read the buildbucket field that is not set for the noop jobs
# actually causes an empty buildbucket message to be set
# TODO(https://crbug.com/1062385) The automatic generation of job IDs causes
# problems when the number of builders with the same name goes from 1 to >1 or
# vice-versa. This generator makes sure both the bucketed and non-bucketed IDs
# work so that there aren't transient failures when the configuration changes
exec('//generators/scheduler-bucketed-jobs.star')
# TODO(https://crbug.com/966115) Run the generator to set the fallback field for
# the empty CQ group until it's exposed in lucicfg or there is a better way to
# create a CQ group for all of the canary branches
exec('//generators/cq-fallback.star')
# TODO(https://crbug.com/819899) There are a number of noop jobs for dummy
# builders defined due to legacy requirements that trybots mirror CI bots
# no-op scheduler jobs are not supported by the lucicfg libraries, so this
# generator adds in the necessary no-op jobs
# The trybots should be update to not require no-op jobs to be triggered so that
# the no-op jobs can be removed
exec('//generators/scheduler-noop-jobs.star')

exec('//validators/builders-in-consoles.star')
