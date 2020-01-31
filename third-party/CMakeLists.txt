# If SKIP_IGNORE_THIRD_PARTY is set then don't try to update
# third-party
if(NOT DISABLE_THIRD_PARTY_UPDATE)
  find_program(GIT git PATHS /bin /usr/bin /usr/local/bin)
  if(NOT GIT)
    message(FATAL_ERROR "git not found")
  endif()
  execute_process(
    COMMAND bash -c "${GIT} submodule status --recursive | cut -c 1-1 | egrep '[+-]'"
    OUTPUT_QUIET
    RESULT_VARIABLE result
    )
  if(result EQUAL 0)
    # Something changed - kill it
    message(
      "--------------------------------------------------\n"
      "third-party changed - resetting it to initial state.  If "
      "you are trying to hack on it make sure to set "
      "-DDISABLE_THIRD_PARTY_UPDATE=ON first.\n"
      "--------------------------------------------------\n"
      )

    execute_process(COMMAND ${GIT} submodule update --init --recursive)
    execute_process(COMMAND ${GIT} submodule foreach --recursive ${GIT} clean -dfx)
    execute_process(COMMAND ${GIT} submodule foreach --recursive ${GIT} reset --hard)
  endif()
endif()

add_subdirectory(boost)
add_subdirectory(gflags)
add_subdirectory(googletest)
add_subdirectory(icu)
add_subdirectory(jemalloc)
add_subdirectory(pcre)
