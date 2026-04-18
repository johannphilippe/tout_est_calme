# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/johann/Documents/git/tout_est_calme/csound_opcode/build/_deps/cycfi_infra-src"
  "/home/johann/Documents/git/tout_est_calme/csound_opcode/build/_deps/cycfi_infra-build"
  "/home/johann/Documents/git/tout_est_calme/csound_opcode/build/_deps/cycfi_infra-subbuild/cycfi_infra-populate-prefix"
  "/home/johann/Documents/git/tout_est_calme/csound_opcode/build/_deps/cycfi_infra-subbuild/cycfi_infra-populate-prefix/tmp"
  "/home/johann/Documents/git/tout_est_calme/csound_opcode/build/_deps/cycfi_infra-subbuild/cycfi_infra-populate-prefix/src/cycfi_infra-populate-stamp"
  "/home/johann/Documents/git/tout_est_calme/csound_opcode/build/_deps/cycfi_infra-subbuild/cycfi_infra-populate-prefix/src"
  "/home/johann/Documents/git/tout_est_calme/csound_opcode/build/_deps/cycfi_infra-subbuild/cycfi_infra-populate-prefix/src/cycfi_infra-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/johann/Documents/git/tout_est_calme/csound_opcode/build/_deps/cycfi_infra-subbuild/cycfi_infra-populate-prefix/src/cycfi_infra-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/johann/Documents/git/tout_est_calme/csound_opcode/build/_deps/cycfi_infra-subbuild/cycfi_infra-populate-prefix/src/cycfi_infra-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
