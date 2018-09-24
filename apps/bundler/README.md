# Bundler Demo App

This directory contains an example application to demonstrate Skip features such as async/await and reactive caching. The application implements a simplified version of a JavaScript "bundler", which takes as input a directory of '.js' files that contain `require('<relative-path>')` statements followed by some source code, and outputs a single file with all `require()`-ed content inlined.

## Running The App

After following the [setup instructions](../../docs/developer/README-cmake.md), run the following from this folder (`<skip-root>/apps/bundler/`):

```
./run.sh --root example
```

This will run the app with the '.js' files in `./example` as input. When prompted, input a path relative to `./example` to see the output (bundled) version. For example, input 'index.js' to see
the output for `./example/index.js`:

```
./run.sh --root example  
Initializing...
Enter file path (relative to working directory):
index.js

<index.js bundle text here>
```

With the application running you can change the contents of the '.js' files in example, re-enter a file path at the prompt again, and view the updated bundled output. Enter `Control-d` to exit.
