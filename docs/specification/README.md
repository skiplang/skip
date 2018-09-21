# Skip Language Specification

The Skip language specification is intended to provide a complete and concise definition of the syntax and semantics for use by developers of a Skip implementation and/or test suite, or developers writing Skip code.

## Format

This specification is written using [Spec Markdown](https://github.com/leebyron/spec-md), a set of markdown specifically designed to write technical specifications. It is based on [Github-flavored Markdown](https://help.github.com/articles/github-flavored-markdown/). If you are familiar with markdown, Spec Markdown should feel comfortable.

Check the [complete listing of the markdown](https://github.com/leebyron/spec-md/blob/master/spec/Markdown.md) and [additional Spec Markdown specific features](https://github.com/leebyron/spec-md/blob/master/spec/SpecAdditions.md) that [Spec Markdown supports](https://github.com/leebyron/spec-md/tree/master/spec).

## Contributing

To contribute, just update the markdown files and [send a pull request](https://github.com/SkipLang/SkipLang/pulls).

`Skip.md` is the primary file for the specification. It contains the front matter information along with a listing of all the chapters that make up the specification. Each chapter will be represented by its own, separate `.md` file.

For example to add a chapter, just add something like this to the bottom of `Skip.md`:

```
# [Introduction](Introduction.md)
```

and then create and update the `Introduction.md` file.

## Setting up environment

You can make changes to the specification markdown files without testing locally, but that is not recommended. It will save you and the reviewers time if you make sure your changes do not break the existing specification before you submit your pull request.

1. Install Node and npm, if not installed already.
  - MacOS: Use [Homebrew](http://brew.sh/) and `brew install node`
  - Linux: Choose your distribution and follow the [package manager instructions](https://nodejs.org/en/download/package-manager/).
  - Windows: Use [Chocolately](https://chocolatey.org/) and `choco install nodejs.install`
1. Make sure you are in the `./specification` directory (the same directory as this `README.md` file).
1. `npm install`

A `node_modules` will have been created and now you are ready to test changes locally.

## Testing and viewing changes locally

### Automatic Test and Build

The `package.json` includes a watcher so that any changes you make to an `.md` file will trigger an automatic retest of your markdown and rebuild of `index.html`, so that you can just refresh the browser to see your changes.

In one command line instance, start the watcher:

```
npm run watch
```

In another command line instance, start the http server:

```
npm run serve
```

### Manual Build and Test

1. Test Your Changes: `npm run test`
1. Build the HTML: `npm run build`
1. View your changes: `npm run serve` and go to `http://127.0.0.1:8080`

> This spec installs a [lightweight http server](https://www.npmjs.com/package/http-server) during the `npm install` process.
