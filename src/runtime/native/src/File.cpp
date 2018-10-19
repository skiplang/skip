/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/File-extc.h"
#include "skip/String.h"
#include "skip/external.h"

#include <iostream>
#include <fstream>

#include <folly/Format.h>

#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>

using namespace skip;

namespace fs = boost::filesystem;

// TODO: T30343087 - validate all bytes read and converted to string data
// are valid utf8. This includes read_line, read_stdin and read_btyes.
static String read_stream_to_string(std::istream& stream) {
  std::vector<char> fileContents(
      (std::istreambuf_iterator<char>(stream)),
      std::istreambuf_iterator<char>());

  return String(fileContents.begin(), fileContents.end());
}

String SKIP_read_stdin(void) {
  return read_stream_to_string(std::cin);
}

String SKIP_read_stdin_bytes(SkipInt bytes) {
  std::string line;
  line.resize(bytes, 0);
  while (bytes > 0 && 0 == std::fread((char*)line.data(), bytes, 1, stdin)) {
  }
  return String{line};
}

String SKIP_read_line(void) {
  std::string line;
  std::getline(std::cin, line);
  if (std::cin.fail()) {
    throwRuntimeError("Error reading from stdin");
  }
  return String{line};
}

String SKIP_open_file(String path) {
  String::CStrBuffer buf;
  std::ifstream file(String(path).c_str(buf), std::ios::binary);
  if (file.fail()) {
    throwRuntimeError(
        "Error opening '%s': %s",
        path.c_str(buf),
        folly::errnoStr(errno).c_str());
  }

  return read_stream_to_string(file);
}

static void
write_text_file_with_mode(String content, String path, const char* mode) {
  String::CStrBuffer buf;
  FILE* f = fopen(path.c_str(buf), mode);
  if (f != nullptr) {
    String::DataBuffer contentBuf;
    auto data = content.slice(contentBuf);

    const int res = fwrite(data.begin(), 1, data.size(), f);
    const int err = errno;

    if (fclose(f)) {
      throwRuntimeError(
          "Error %s closing file '%s'",
          folly::errnoStr(errno).c_str(),
          path.c_str(buf));
    }
    if (res != data.size()) {
      throwRuntimeError(
          "Error %s writing to file '%s'",
          folly::errnoStr(err).c_str(),
          path.c_str(buf));
    }
  } else {
    throwRuntimeError(
        "Error %s opening file '%s'",
        folly::errnoStr(errno).c_str(),
        path.c_str(buf));
  }
}

void SKIP_string_to_file(String content, String path) {
  write_text_file_with_mode(content, path, "w");
}

void SKIP_FileSystem_appendTextFile(String path, String content) {
  write_text_file_with_mode(content, path, "a");
}

void SKIP_FileSystem_ensure_directory(String path) {
  boost::system::error_code ec;
  fs::create_directory(path.toCppString());
  if (ec)
    SKIP_throwRuntimeError(String{ec.message()});
}

bool SKIP_FileSystem_exists(String path) {
  boost::system::error_code ec;
  return fs::exists(path.toCppString(), ec);
}

bool SKIP_FileSystem_is_directory(String path) {
  boost::system::error_code ec;
  return fs::is_directory(path.toCppString(), ec);
}

RObj* SKIP_FileSystem_readdir(String path) {
  boost::system::error_code ec;
  std::vector<fs::directory_entry> listdir;
  std::copy(
      fs::directory_iterator(path.toCppString(), ec),
      fs::directory_iterator(),
      std::back_inserter(listdir));
  auto res = createStringVector(listdir.size());

  auto out = res->begin();
  for (auto i : listdir) {
    *out++ = String{i.path().filename().string()};
  }

  return res;
}
