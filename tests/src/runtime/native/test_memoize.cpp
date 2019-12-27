/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// TODO: re-enable those tests.

/* What happened?
 *
 * In an attempt to simplify the build, I am trying to remove the
 * dependence on folly. I tried to support it for a few months,
 * but it's just too complicated, it breaks often ...
 *
 * The problem is that we rely on folly::Future for the implementation
 * of async/await in Skip.
 *
 * The good news is that the implementation in the runtime could be replaced
 * with the std::future provided by C++17. The bad news is that folly provides
 * primitives not implemented by the standard (then, thenElse etc ...), which
 * forces me to disable the memoization tests (they rely a lot on those).
 *
 * So for the time being, I am disabling these tests. I will revisit this once
 * SKIP builds everywhere (which I think is more important). I will also create
 * an issue in github to keep track of this.
 *
 */
