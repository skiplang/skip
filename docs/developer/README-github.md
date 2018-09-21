# Github Workflow

## One time steps

1. Create a github fork.

    On github click the "Fork" button on the top-right of the SkipLang page.

    ![Fork Button](README-github/fork.png)

1. Copy a clone of your fork locally:

        $ git clone git@github.com:<username>/skip.git skip
        $ cd skip

1. Add the main SkipLang repo as a remote named 'upstream':

        $ git remote add upstream git@github.com:skiplang/skip.git
        $ git remote set-url --push upstream nopush
        $ git fetch upstream

    The 'remote set-url' statement ensures that you can't accidentally push
    directly to upstream.

1. Add our commit hooks to your git repo:

        $ rm -rf .git/hooks
        $ ln -s $(cd tools/git-hooks && pwd) .git/hooks

## Repeated steps

### Once you've got the updated code you're going to want to make changes.

This is one way to deal with github flow.  There are other perfectly
valid options such as not bothering with branches or reusing branches.

1. Create a branch to work on:

        $ git checkout master
        $ git pull upstream master --rebase
        $ git branch <branch name>
        $ git checkout <branch name>

1. Modify code

    Some useful commands:

        $ git diff                # hg diff
        $ git status              # hg status
        $ git checkout <filename> # hg revert <filename>
        $ git reset --hard        # hg revert -a

    The [Git Hg Rosetta
    Stone](https://github.com/sympy/sympy/wiki/git-hg-rosetta-stone)
    can be a useful reference for Mercurial users.

1. Commit:

        $ git commit -a -m 'This is my message'

    If you have new files you need to add them with 'git add'.  Unlike
    hg, git won't automatically commit new files.

    If you're modifying runtime code you may need to sync that code.
    See the section below [Syncing Runtimes](#syncing-runtimes) and
    [Updating LKG](#updating-lkg).

1. Push your changes to your forked repo:

        $ git push origin <branch name>

1. On github go to your fork, select your branch and click "New pull
request".  You should see a diff where you can add reviewers and then
create a PR.  Reviewers should get an email and can submit comments.

    ![Fork Button](README-github/pr.png)

1. Once the PR is accepted then at the bottom of the PR click the
"Squash and merge" button (you may need to change the mode from
"Rebase and merge").  This will merge your changes into the master
repo.

    ![Fork Button](README-github/squash.png)

1. Delete your local branch:

        $ git branch -D <branch name>

1. Delete your remote branch (this can also be done on the github UI):

        $ git push origin :<branch name>

### Dealing with rejected reviews on GitHub

If your review is rejected (or if you want to update it anyway) make
your changes locally.  Then commit and push your changes:

    $ git commit -a -m 'Updated my review'
    $ git push origin <branch name>

Return to the github PR and mark the rejected reviewer as updated
(reselect that reviewer on the PR) so they get an email.

Note that this pushes a stacked diff to github.  
