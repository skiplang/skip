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
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

using namespace skip;

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
        "Error opening '%s': %s", path.c_str(buf), std::strerror(errno));
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
          "Error %s closing file '%s'", std::strerror(errno), path.c_str(buf));
    }
    if (res != data.size()) {
      throwRuntimeError(
          "Error %s writing to file '%s'", std::strerror(err), path.c_str(buf));
    }
  } else {
    throwRuntimeError(
        "Error %s opening file '%s'", std::strerror(errno), path.c_str(buf));
  }
}

void SKIP_string_to_file(String content, String path) {
  write_text_file_with_mode(content, path, "w");
}

void SKIP_FileSystem_appendTextFile(String path, String content) {
  write_text_file_with_mode(content, path, "a");
}

void SKIP_FileSystem_ensure_directory(String path) {
  int ec;
  ec = mkdir(path.toCppString().c_str(), 0777);
  if (ec)
    SKIP_throwRuntimeError(String{std::strerror(ec)});
}

bool SKIP_FileSystem_exists(String name) {
  return (access(name.toCppString().c_str(), F_OK) != -1);
}

bool SKIP_FileSystem_is_directory(String path) {
  struct stat path_stat;
  stat(path.toCppString().c_str(), &path_stat);
  return S_ISDIR(path_stat.st_mode);
}

RObj* SKIP_FileSystem_readdir(String path) {
  DIR* dir;
  struct dirent* ent;
  std::vector<String> listdir;

  if ((dir = opendir(path.toCppString().c_str())) != NULL) {
    /* print all the files and directories within directory */
    while ((ent = readdir(dir)) != NULL) {
      if (ent->d_name[0] == '.') {
        if (ent->d_name[1] == 0) {
          continue;
        }
        if (ent->d_name[1] == '.') {
          if (ent->d_name[2] == 0) {
            continue;
          }
        }
      }
      listdir.push_back(String{ent->d_name});
    }
    closedir(dir);
  } else {
    /* could not open directory */
    SKIP_throwRuntimeError(
        String{"Could not open directory: " + path.toCppString()});
  }

  auto res = createStringVector(listdir.size());
  auto out = res->begin();
  for (auto i : listdir) {
    *out++ = i;
  }

  return res;
}
