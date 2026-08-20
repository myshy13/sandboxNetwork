# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/hamish/Desktop/Code/sandboxNetwork/build/_deps/cereal-src")
  file(MAKE_DIRECTORY "/Users/hamish/Desktop/Code/sandboxNetwork/build/_deps/cereal-src")
endif()
file(MAKE_DIRECTORY
  "/Users/hamish/Desktop/Code/sandboxNetwork/build/_deps/cereal-build"
  "/Users/hamish/Desktop/Code/sandboxNetwork/build/_deps/cereal-subbuild/cereal-populate-prefix"
  "/Users/hamish/Desktop/Code/sandboxNetwork/build/_deps/cereal-subbuild/cereal-populate-prefix/tmp"
  "/Users/hamish/Desktop/Code/sandboxNetwork/build/_deps/cereal-subbuild/cereal-populate-prefix/src/cereal-populate-stamp"
  "/Users/hamish/Desktop/Code/sandboxNetwork/build/_deps/cereal-subbuild/cereal-populate-prefix/src"
  "/Users/hamish/Desktop/Code/sandboxNetwork/build/_deps/cereal-subbuild/cereal-populate-prefix/src/cereal-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/hamish/Desktop/Code/sandboxNetwork/build/_deps/cereal-subbuild/cereal-populate-prefix/src/cereal-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/hamish/Desktop/Code/sandboxNetwork/build/_deps/cereal-subbuild/cereal-populate-prefix/src/cereal-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
