def _ensure_ci_jobs_can_be_bucketed_or_not(ctx):
  cfg = ctx.output['luci-scheduler.cfg']

  ci_jobs = {}
  for j in cfg.job:
    # The default behavior is the same as 'triggered', so look for non-empty
    # schedules that are not 'triggered', we don't want to create duplicates of
    # those jobs because it will cause double the number of builds to be
    # scheduled
    if j.schedule and j.schedule != 'triggered':
      continue

    bucket = j.buildbucket.bucket
    if bucket != 'luci.chromium.ci':
      continue

    builder = j.buildbucket.builder
    if builder in ci_jobs:
      fail('Multiple jobs defined for CI builder ci/{}'.format(builder))

    ci_jobs[builder] = j

  for builder, job in ci_jobs.items():
    new_job = proto.from_wirepb(proto.message_type(job), proto.to_wirepb(job))
    new_job.id = builder if job.id != builder else 'ci-{}'.format(builder)
    cfg.job.append(new_job)

  jobs = sorted(cfg.job, key=lambda j: j.id)
  cfg.job.clear()
  cfg.job.extend(jobs)

lucicfg.generator(_ensure_ci_jobs_can_be_bucketed_or_not)
