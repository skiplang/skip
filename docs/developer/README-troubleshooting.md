# Troubleshooting cmake, ninja, and build tooling

## Busted `third-party/`

If `third-party/` is corrupted - you accidentally changed files in that directory or are missing
files - reset with:

```
rm -rf third-party
git checkout third-party
git submodule update --init --recursive
```

Example issues:
- If cmake fails with lines such as "third-party/folly/src ... is not an existing non-empty directory", reset third-party as above.
