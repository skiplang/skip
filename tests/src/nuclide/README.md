# Creating a New LSP Test

## Add New Test Source Files
1. Start with a clean check out
- Add new Skip sources to `tests/src/nuclide/lsp/src`. Be sure and include any `skip.project.json` files.
- Commit your changes. `git status` should report no changes.

## Record a LSP Log in Nuclide
1. Close all Skip files in Nuclide
- shutdown Nuclide
- rm /tmp/skip_lsp*
- restart Nuclide. There should be no open .sk files, if there are open .sk files, go back to step 2.
- Ensure that `ls /tmp/skip_lsp*` yields no results. If it does, go back to step 2.
- Open your test source from `tests/src/nuclide/lsp/src` in Nuclide. Do not open any unrelated .sk files/projects while recording a test log. If you do, best go back to step 1.
- Perform your test exercising various Nuclide features including saving file changes.
- Close Nuclide
- `git reset --hard HEAD` to revert any changes you made to the initial test files.

## Convert the Nuclide LSP Log to a LSP Test Script
1. Find the log file in `/tmp/skip_lsp*` for the project you care about.
- Run `tests/src/nuclide/tools/log_to_script.sh <log-file-name> `pwd`/tests/src/nuclide/lsp/src | json_pp > tests/src/nuclide/<new-test-script>.script.json`
- `git add tests/src/nuclide/<new-test-script>.script.json && git commit -m 'Add new LSP test'`

At this point, your `git status` should be clean, the new test is committed.
You can try out the new test with:

`cmake . && ninja test_skip_lsp`
