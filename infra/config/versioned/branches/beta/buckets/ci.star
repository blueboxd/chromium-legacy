load('//versioned/vars/ci.star', 'vars')
vars.bucket.set('ci-beta')

load('//versioned/milestones.star', milestone='beta')
exec('//versioned/milestones/%s/buckets/ci.star' % milestone)
