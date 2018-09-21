# Instructions for various CircleCI things.

## Setting up a CircleCI account for a new user

First set the user up as an account with access on github.

Then go to the [Team Page](https://circleci.com/team/gh/SkipLang) on
CircleCI.  Click 'Invite Teamates' and select the person to invite
them.

## Changing the docker image

We use the docker image [aorenste/skip-ci](https://hub.docker.com/r/aorenste/skip-ci/) as our base.  This is a public image so don't add anything Facebook private in it.

The docker image is defined in `.circleci/docker/skip/Dockerfile`.  This can be rebuilt by running the script `.circleci/docker/build`.
