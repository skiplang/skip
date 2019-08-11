add_compile_options(-Wmissing-declarations)
if(CMAKE_COMPILER_IS_GNUCXX AND
    CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 7.0)
  add_compile_options(-faligned-new)
endif()

add_subdirectory(benchmarks)
add_subdirectory(frontend)
add_subdirectory(native)
add_subdirectory(nuclide)
add_subdirectory(runtime)
add_subdirectory(serializationGenerator)
add_subdirectory(project)
