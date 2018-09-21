macro(_wrapList VAR)
  string(REPLACE ";" "\\;" ${VAR} "${${VAR}}")
endmacro()

macro(joinListBySpace OUT)
  string(REPLACE ";" " " ${OUT} "${ARGN}")
endmacro()

# This crazy function allows us to parse arguments like standard cmake commands.
#     set(VAR1)
#     set(VAR2)
#     set(VAR3)
#     _parseArgs(VAR1 VAR2 VAR3 ARGN ${ARGN})
#
# This will parse your function arguments, such that the command:
#
#     myfn(VAR1 a b c VAR2 d e f VAR3 g h i)
#
# Will put "a;b;c" into VAR1, "d;e;f" into VAR2 and "g;h;i" into VAR3.
#
# If a variable name starts with "=" then it is a boolean - the existence of
# that name will set it to true.
#
# If a variable name starts with "+" then it is a "multi" and can be specified
# multiple times. In that case the variable will be set to the LIST of LISTS of
# strings - note that this is a complex concept but commands like list() and
# foreach() should work properly, yielding a list of the args.
#
macro(_parseArgs)
  set(macroArgs "${ARGN}")

  list(FIND macroArgs "ARGN" ARGN_LOC)
  if(ARGN_LOC EQUAL -1)
    message(FATAL_ERROR "Missing 'ARGN' delimiter")
  endif()

  set(VARS)
  set(UNARY_VARS)
  set(MULTI_VARS)
  math(EXPR ARGN_LOC "${ARGN_LOC} - 1")
  foreach(n RANGE ${ARGN_LOC})
    list(GET macroArgs 0 head)
    list(REMOVE_AT macroArgs 0)
    string(FIND ${head} "=" isUnary)
    string(FIND ${head} "+" isMulti)
    if(NOT isUnary EQUAL -1)
      string(SUBSTRING ${head} 1 -1 head)
      list(APPEND UNARY_VARS ${head})
    elseif(NOT isMulti EQUAL -1)
      string(SUBSTRING ${head} 1 -1 head)
      list(APPEND MULTI_VARS ${head})
    else()
      list(APPEND VARS ${head})
    endif()
  endforeach()
  list(REMOVE_AT macroArgs 0)

  set(curvar)
  set(curmulti)
  set(SEEN)
  foreach(arg ${macroArgs})
    list(FIND SEEN ${arg} seen)
    if(NOT seen EQUAL -1)
      message(FATAL_ERROR "Arg ${arg} used multiple times")
    endif()
    list(FIND VARS ${arg} arg_var_loc)
    list(FIND UNARY_VARS ${arg} arg_unary_loc)
    list(FIND MULTI_VARS ${arg} arg_multi_loc)
    if(NOT arg_var_loc EQUAL -1 OR NOT arg_multi_loc EQUAL -1 OR NOT arg_unary_loc EQUAL -1)
      if(curmulti AND NOT "${curvar}" STREQUAL "")
        _wrapList(${curvar})
        list(APPEND ${curmulti} "${${curvar}}")
        set(${curvar})
      endif()
      set(curvar)
      set(curmulti)
      if(NOT arg_var_loc EQUAL -1)
        set(curvar ${arg})
        list(APPEND SEEN ${arg})
      elseif(NOT arg_multi_loc EQUAL -1)
        set(curvar ${arg}_)
        set(curmulti ${arg})
        # Don't add to SEEN
      elseif(NOT arg_unary_loc EQUAL -1)
        set(${arg} TRUE)
        list(APPEND SEEN ${arg})
      endif()
    else()
      if(NOT "${curvar}" STREQUAL "")
        list(APPEND ${curvar} ${arg})
      else()
        message(FATAL_ERROR "Unexpected parameter ${arg}")
      endif()
    endif()
  endforeach()
  if(curmulti AND NOT "${curvar}" STREQUAL "")
    _wrapList(${curvar})
    list(APPEND ${curmulti} "${${curvar}}")
  endif()
endmacro(_parseArgs)


function(regex_replace_list TARGET REGEX REPL LIST)
  set(result "")
  foreach(f ${LIST} ${ARGN})
    string(REGEX REPLACE ${REGEX} ${REPL} f2 ${f})
    list(APPEND result ${f2})
  endforeach()
  set(${TARGET} ${result} PARENT_SCOPE)
endfunction()


function(lcfirst_list TARGET LIST)
  set(result "")
  foreach(f ${LIST} ${ARGN})
    string(SUBSTRING ${f} 0 1 cdr)
    string(TOLOWER ${cdr} cdr)
    string(SUBSTRING ${f} 1 -1 cond)
    list(APPEND result "${cdr}${cond}")
  endforeach()
  set(${TARGET} ${result} PARENT_SCOPE)
endfunction()


function(mkparents PATH)
  get_filename_component(parent ${PATH} DIRECTORY)
  file(MAKE_DIRECTORY ${parent})
endfunction()


function(debug_dump_props target)
  if(NOT CMAKE_PROPERTY_LIST)
    execute_process(COMMAND cmake --help-property-list OUTPUT_VARIABLE CMAKE_PROPERTY_LIST)
    string(REGEX REPLACE ";" "\\\\;" CMAKE_PROPERTY_LIST "${CMAKE_PROPERTY_LIST}")
    string(REGEX REPLACE "\n" ";" CMAKE_PROPERTY_LIST "${CMAKE_PROPERTY_LIST}")
    set(CMAKE_PROPERTY_LIST ${CMAKE_PROPERTY_LIST} CACHE STRING "for debug_dump_props")
    mark_as_advanced(CMAKE_PROPERTY_LIST)
  endif()

  if(NOT TARGET ${target})
    message(FATAL_ERROR "Invalid target ${target}")
  endif()

  foreach(prop ${CMAKE_PROPERTY_LIST})
    get_target_property(propval ${target} TYPE)
    if(propval STREQUAL INTERFACE_LIBRARY AND NOT (prop MATCHES "^INTERFACE_" OR prop MATCHES "^IMPORTED$"))
      continue()
    endif()
    if(prop MATCHES LOCATION)
      continue()
    endif()

    string(REPLACE "<CONFIG>" "Debug" prop ${prop})
    get_property(propval TARGET ${target} PROPERTY ${prop} SET)
    if(propval)
      get_target_property(propval ${target} ${prop})
      message("${target} ${prop} = ${propval}")
    endif()
  endforeach()
endfunction()

if(NOT WIN32)
  string(ASCII 27 Esc)
  set(COLOR_NORMAL "${Esc}[m")
  set(COLOR_RED    "${Esc}[31m")
  set(COLOR_GREEN  "${Esc}[32m")
  set(COLOR_BLUE   "${Esc}[34m")
  set(COLOR_WHITE  "${Esc}[37m")
endif()


# add_command_target() adds custom build rule which executes COMMAND when the
# dependencies are out of date.
#
# Unlike add_custom_command() add_command_target() is an actual target which can
# be depended on outside the file which defines it.
#
# Unlike add_custom_target(), add_command_target() has an output file which will
# cause dependents to rebuild and can be used with $<TARGET_FILE:tgt>.
#
function(add_command_target Name)
  set(OUTPUT)
  set(COMMAND)
  set(DEPENDS)
  set(ALL)
  set(WORKING_DIRECTORY)
  _parseArgs("=ALL" OUTPUT "+COMMAND" DEPENDS WORKING_DIRECTORY ARGN ${ARGN})

  if(ALL)
    set(all ALL)
  else()
    set(all)
  endif()

  if(NOT OUTPUT)
    message(FATAL_ERROR "Missing OUTPUT")
  endif()
  if(NOT COMMAND)
    message(FATAL_ERROR "Missing COMMAND")
  endif()
  if(OUTPUT STREQUAL "${CMAKE_CURRENT_BINARY_DIR}/${Name}")
    message(FATAL_ERROR "Because of cmake limitations add_custom_target cannot have a target with the same name as its output")
  endif()

  if(WORKING_DIRECTORY)
    set(WORKING_DIRECTORY WORKING_DIRECTORY ${WORKING_DIRECTORY})
  endif()

  set(COMMANDS)
  foreach(C ${COMMAND})
    set(COMMANDS ${COMMANDS} COMMAND ${C})
  endforeach()

  get_filename_component(OUTPUT_NAME ${OUTPUT} NAME)
  get_filename_component(OUTPUT_DIR ${OUTPUT} DIRECTORY)

  add_custom_command(OUTPUT ${OUTPUT} ${COMMANDS} ${WORKING_DIRECTORY} DEPENDS ${DEPENDS})
  add_custom_target(_intermediate_.${Name} ${all} DEPENDS ${OUTPUT})

  add_library(${Name} UNKNOWN IMPORTED GLOBAL)
  add_dependencies(${Name} _intermediate_.${Name})
  set_target_properties(${Name} PROPERTIES
    IMPORTED_LOCATION ${OUTPUT}
    OUTPUT_NAME ${OUTPUT_NAME}
    RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR}
  )

  add_custom_target(${Name}.dep DEPENDS ${Name})
endfunction()


function(globSources VAR)
  file(GLOB result ${ARGN})
  foreach(fname ${result})
    if(fname MATCHES "/\\.#" OR fname MATCHES "^\\.#" OR fname MATCHES "~$")
      list(REMOVE_ITEM result ${fname})
    endif()
  endforeach()
  set(${VAR} ${result} PARENT_SCOPE)
endfunction()


function(recursiveGlobSources VAR)
  file(GLOB_RECURSE result ${ARGN})
  foreach(fname ${result})
    if(fname MATCHES "/\\.#" OR fname MATCHES "^\\.#" OR fname MATCHES "~$")
      list(REMOVE_ITEM result ${fname})
    endif()
  endforeach()
  set(${VAR} ${result} PARENT_SCOPE)
endfunction()
