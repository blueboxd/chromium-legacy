def _find_empty_cq_group(ctx):
  cq_cfg = ctx.output['commit-queue.cfg']

  for c in cq_cfg.config_groups:
    if c.name == 'fallback-empty-cq':
      return c

  fail('Could not find empty CQ group')

def _generate_cq_group_fallback(ctx):
  cq_group = _find_empty_cq_group(ctx)
  cq_group.fallback = 1  # YES


lucicfg.generator(_generate_cq_group_fallback)
