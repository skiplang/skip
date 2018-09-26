/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

const React = require("react");

const githubButton = (
  <a
    className="github-button"
    href="https://github.com/SkipLang/SkipLang"
    data-icon="octicon-star"
    data-count-href="/SkipLang/SkipLang/stargazers"
    data-count-api="/repos/SkipLang/SkipLang#stargazers_count"
    data-count-aria-label="# stargazers on GitHub"
    aria-label="Star this project on GitHub"
  >
    Star
  </a>
);

class Footer extends React.Component {
  render() {
    const currentYear = new Date().getFullYear();
    return (
      <footer className="nav-footer" id="footer">
        <section className="sitemap">
          <div>
            <h5>Docs</h5>
            <a href={this.props.config.baseUrl + "docs/tutorial.html"}>
              Tutorial
            </a>
            <a href={this.props.config.baseUrl + "docs/getting_started.html"}>
              Getting Started
            </a>
            <a href={this.props.config.baseUrl + "docs/string.html"}>
              Standard Library
            </a>
          </div>
          <div>
            <h5>Tools</h5>
            <a href={this.props.config.baseUrl + "playground"}>
              Playground
            </a>
            <a href={this.props.config.baseUrl + "docs/assets.html"}>
              Logo & Assets
            </a>
          </div>
          <div>
            <h5>Community</h5>
            <a href="https://github.com/skiplang/skip">
              GitHub
            </a>
          </div>
        </section>

        <section className="copyright">
          Copyright &copy; {currentYear} Facebook Inc.
        </section>
      </footer>
    );
  }
}

module.exports = Footer;
