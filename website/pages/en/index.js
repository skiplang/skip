/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

const React = require("react");

const CompLibrary = require("../../core/CompLibrary.js");
const Container = CompLibrary.Container;
const GridBlock = CompLibrary.GridBlock;

const siteConfig = require(process.cwd() + "/siteConfig.js");

class Button extends React.Component {
  render() {
    return (
      <div className="pluginWrapper buttonWrapper">
        <a className="button" href={this.props.href} target={this.props.target}>
          {this.props.children}
        </a>
      </div>
    );
  }
}

Button.defaultProps = {
  target: "_self"
};

class HomeSplash extends React.Component {
  render() {
    return (
      <div className="homeContainer">
        <script src="redirect.js" />
        <div className="homeSplashFade">
          <div className="wrapper homeWrapper">
            <div className="inner">
              <h1 className="projectTitle">
                Skip
                <small>
                  {siteConfig.tagline}
                </small>
              </h1>
              <div className="section promoSection">
                <div className="promoRow">
                  <div className="pluginRowBlock">
                    <Button href={siteConfig.baseUrl + "docs/tutorial.html"}>
                      Tutorial
                    </Button>
                    <Button href={siteConfig.baseUrl + "playground/"}>
                      Playground
                    </Button>
                  </div>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>
    );
  }
}

class Index extends React.Component {
  render() {
    let language = this.props.language || "en";

    return (
      <div className="home">
        <HomeSplash language={language} />
        <div className="mainContainer">
          <Container className="homeProjectStatus">
            <h2>Project Status</h2>
            <p>Skip is an experimental programming language developed at Facebook from 2015-2018.</p>
            <p>Skip's primary goal as a research project was to explore language and runtime support for correct, efficient memoization-based caching and cache invalidation. Skip achieved this via a static type system that carefully tracks mutability, while also supporting modern language features such as traits, generics and subtyping.</p>
            <p>The Skip project concluded in 2018 and Skip is no longer under active development at Facebook. </p>
            <p>Facebook is making the implementation available under a permissive open source license to enable the programming language research community to study and build on the design and implementation of the language, compiler and libraries. The language, compiler and libraries are maintained as a side project by Julien Verlaguet, the main designer of the language.</p>
          </Container>
          <Container className="homeOverview">
            <h2>Skip Overview</h2>
            <p>
              Skip is a general-purpose programming language that tracks side effects to provide caching with reactive invalidation, ergonomic and safe parallelism, and efficient garbage collection. Skip is statically typed and ahead-of-time compiled using LLVM to produce highly optimized executables.
            </p>
          </Container>
          <Container className="homeFeature">
            <h3>Caching with Reactive Invalidation</h3>
            <p>
              Skip's main new language feature is its precise tracking of side effects, including both mutability of values as well as distinguishing between non-deterministic data sources and those that can provide reactive invalidations (telling Skip when data has changed). When Skip's type system can prove the absence of side effects at a given function boundary, developers can opt-in to safely memoizing that computation, with the runtime ensuring that previously cached values are invalidated when underlying data changes.
            </p>
          </Container>
          <Container className="homeFeature">
            <h3>Safe Parallelism</h3>
            <p>
              Skip supports two complementary forms of concurrent programming, both of which avoid the usual thread safety issues thanks to Skip's tracking of side effects. First, Skip supports ergonomic asynchronous computation with async/await syntax. Thanks to Skip's tracking of side effects, asynchronous computations cannot refer to mutable state and are therefore safe to execute in parallel (so independent async continuations can continue in parallel). Second, Skip has APIs for direct parallel computation, again using its tracking of side effects to prevent thread safety issues such as shared access to mutable state.
            </p>
          </Container>
          <Container className="homeFeature">
            <h3>Efficient and Predictable GC</h3>
            <p>
              Skip uses a novel approach to memory management that combines aspects of typical garbage collectors with more straightforward linear (bump) allocation schemes. Thanks to Skip's tracking of side effects the garbage collector only has to scan memory reachable from the root of a computation. In practical terms, this means that developers can write code with predictable GC overhead.
            </p>
          </Container>
          <Container className="homeFeature">
            <h3>Hybrid Functional/Object-Oriented Language</h3>
            <p>
            Skip features an opinionated mix of ideas from functional and object-oriented styles, all carefully integrated to form a cohesive language. Like functional languages, Skip is expression-oriented and supports abstract data types, pattern matching, easy lambdas, higher-order functions, and (optionally) enforcing pure/referentially-transparent API boundaries. Like imperative/OO languages, Skip supports classes with inheritance, mutable objects, loops, and early returns. Skip also incorporates ideas from “systems” languages to support low-overhead abstractions, compact memory layout of objects via value classes, and patterns that ensure code specialization with static method dispatch.
            </p>
          </Container>
          <Container className="homeFeature">
            <h3>Great Developer Experience</h3>
            <p>
            Skip was designed from the start to support a great developer experience, with a rapid iteration speed more commonly associated with dynamic languages. The compiler supports incremental type-checking (with alpha versions of IDE plugins providing near-instantaneous errors as you type), provides hints for common syntax mistakes and to help newcomers learn the language, recognizes small typos of method/class names, and even recognizes common alternatives to Skip's standard library method names and suggests the correct name in Skip. Skip also features a code-formatter to ensure consistent code style and a tool for running codemods.
            </p>
          </Container>
          <Container className="homeFeature">
            <h3>Built by a Team of Veterans</h3>
            <p>
            Skip was designed by an experienced team including senior contributors to ActionScript, C#, Flow, Hack, HHVM, Prettier, React Native, and Relay.
            </p>
          </Container>
        </div>
      </div>
    );
  }
}

module.exports = Index;
