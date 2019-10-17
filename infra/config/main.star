#!/usr/bin/env lucicfg
# See https://chromium.googlesource.com/infra/luci/luci-go/+/HEAD/lucicfg/doc/README.md
# for information on starlark/lucicfg

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

# Copy the not-yet migrated files to the generated outputs
# TODO(https://crbug.com/1011908) Migrate the configuration in these files to starlark
[lucicfg.emit(dest = f, data = io.read_file(f)) for f in (
    'luci-milo.cfg',
    # TODO(https://crbug.com/1015148) lucicfg generates luci-notify.cfg very
    # differently from our hand-written file and doesn't do any normalization
    # for luci-notify.cfg so the semantic diff is large and confusing
    'luci-notify.cfg',
    # TODO(https://crbug.com/819899) There are a number of noop jobs for dummy
    # builders defined due to legacy requirements that trybots mirror CI bots
    # and noop scheduler jobs cannot be created in lucicfg, so the trybots need
    # to be updated to not rely on dummy builders and the noop jobs need to be
    # removed
    'luci-scheduler.cfg',
)]

# Just copy tricium-prod.cfg to the generated outputs
lucicfg.emit(
    dest = 'tricium-prod.cfg',
    data = io.read_file('tricium-prod.cfg'),
)

luci.project(
    name = 'chromium',
    buildbucket = 'cr-buildbucket.appspot.com',
    logdog = 'luci-logdog.appspot.com',
    swarming = 'chromium-swarm.appspot.com',
    acls = [
        acl.entry(
            roles = [
                acl.LOGDOG_READER,
                acl.PROJECT_CONFIGS_READER,
            ],
            groups = 'all',
        ),
        acl.entry(
            roles = acl.LOGDOG_WRITER,
            groups = 'luci-logdog-chromium-writers',
        ),
    ],
)

luci.cq(
    submit_max_burst = 2,
    submit_burst_delay = time.minute,
    status_host = 'chromium-cq-status.appspot.com',
)

luci.logdog(
    gs_bucket = 'chromium-luci-logdog',
)

exec('//buckets/ci.star')
exec('//buckets/findit.star')
exec('//buckets/try.star')
exec('//buckets/webrtc.star')
exec('//buckets/webrtc.fyi.star')

exec('//cq-builders-md.star')
