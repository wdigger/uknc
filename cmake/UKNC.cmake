# UKNC.cmake -- the build rules this machine needs on top of plain CMake.
#
# Included by uknc-toolchain.cmake, so a project that names that file as
# its CMAKE_TOOLCHAIN_FILE has all of this already.
#
#   uknc_add_program(hello hello.c)         # HELLO.SAV, and hello.elf
#   uknc_add_ppu_module(gfppu gfourppu.c)   # GFPPU.PPU, and gfppu.ppu.elf
#   uknc_add_disk(disk CONTENTS hello)      # disk.dsk with RT-11 on it
#   uknc_add_run(hello)                     # `cmake --build . -t run-hello`
#
# Each of these is an ordinary CMake target afterwards: say
# target_link_libraries(hello PRIVATE ppu pdp11), target_compile_options,
# target_sources and the rest as usual, and what they add reaches the
# .elf twin too -- it is linked from the same objects, and inherits the
# same libraries.

# --------------------------------------------------------------------
# What RT-11 will accept as a file name.
#
# Six characters and a three-character type, upper case, no punctuation.
# rt11dsk does not check: it takes whatever it is handed, keeps the
# first six characters and writes a catalogue entry for a name nobody
# asked for -- which is only noticed later, when RT-11 cannot find the
# file under the name it was built as.
function(_uknc_check_rt11_name name what)
  string(LENGTH "${name}" length)
  if(length GREATER 6 OR NOT name MATCHES "^[A-Za-z0-9]+$")
    message(WARNING
      "${what}: \"${name}\" is not a name RT-11 can keep -- at most six "
      "letters and digits.  Give it one with NAME, or rt11dsk will "
      "shorten it for you.")
  endif()
endfunction()

# A tool that is a shell script rather than a program: sh has to run it
# where the shell is not what runs commands anyway.  This is what makes
# the PPU linker and uknc-run work from a Windows build that is not
# being driven from MSYS2.
function(_uknc_script_command var script)
  if(CMAKE_HOST_WIN32 AND NOT script MATCHES "\\.exe$")
    find_program(UKNC_SH NAMES sh bash)
    if(NOT UKNC_SH)
      message(FATAL_ERROR
        "${script} is a shell script and there is no sh on PATH to run it. "
        "Build from an MSYS2 or Git Bash shell, or set UKNC_SH.")
    endif()
    set(${var} "${UKNC_SH}" "${script}" PARENT_SCOPE)
  else()
    set(${var} "${script}" PARENT_SCOPE)
  endif()
endfunction()

# --------------------------------------------------------------------
# uknc_add_program(<target> [sources...]
#                  [NAME <rt11name>] [NO_ELF] [NO_MAP])
#
# A program for the CPU side: <target> builds <name>.sav, the flat image
# RT-11 loads, and <target>-elf builds <name>.elf beside it.
#
# The twin is the same program with its symbols and DWARF kept, which a
# .sav has nowhere to put -- it is what gdb and the VS Code extension
# read.  It is linked from the very objects the .sav was linked from, so
# there is no way for the two to drift apart.
#
# NAME sets what the file is called when the target's own name is not a
# name RT-11 can keep (six characters).
function(uknc_add_program target)
  cmake_parse_arguments(arg "NO_ELF;NO_MAP" "NAME" "" ${ARGN})

  set(name "${target}")
  if(arg_NAME)
    set(name "${arg_NAME}")
  endif()
  _uknc_check_rt11_name("${name}" "uknc_add_program(${target})")

  add_executable(${target} ${arg_UNPARSED_ARGUMENTS})
  set_target_properties(${target} PROPERTIES OUTPUT_NAME "${name}")

  # Where every byte went, which on a machine with 64 KB of address
  # space is read rather more often than it is on a desktop.
  if(NOT arg_NO_MAP)
    target_link_options(${target} PRIVATE
      "-Wl,-Map=$<TARGET_FILE_NAME:${target}>.map")
  endif()

  if(NOT arg_NO_ELF)
    add_executable(${target}-elf $<TARGET_OBJECTS:${target}>)
    set_target_properties(${target}-elf PROPERTIES
      OUTPUT_NAME "${name}" SUFFIX ".elf" LINKER_LANGUAGE C)
    # Same objects, same addresses, same libraries -- only the output
    # format differs, and the debug sections are not ALLOC, so nothing
    # about them reaches the image.
    target_link_options(${target}-elf PRIVATE -Wl,-m,pdp11rt11)
    target_link_libraries(${target}-elf PRIVATE
      $<TARGET_PROPERTY:${target},LINK_LIBRARIES>)
  endif()
endfunction()

# --------------------------------------------------------------------
# uknc_add_ppu_module(<target> [sources...]
#                     [NAME <rt11name>] [LIBRARIES ...] [NO_ELF])
#
# A program for the PPU side: <name>.ppu, the RT-11 object module that
# the CPU side hands to ppuc_load_code(), and <name>.ppu.elf beside it
# for gdb (see libs/libppu/ppu.gdb, which puts it over the running
# module at the address it was loaded to).
#
# Not an add_executable: a .ppu is a relocatable object module, not an
# image, and pdp11-uknc-rt11-ld-ppu is what makes one.  The sources are
# compiled as an object library and handed to it.
function(uknc_add_ppu_module target)
  cmake_parse_arguments(arg "NO_ELF" "NAME" "LIBRARIES" ${ARGN})

  set(name "${target}")
  if(arg_NAME)
    set(name "${arg_NAME}")
  endif()
  _uknc_check_rt11_name("${name}" "uknc_add_ppu_module(${target})")

  set(objects ${target}_objects)
  add_library(${objects} OBJECT ${arg_UNPARSED_ARGUMENTS})

  # -Os, and not the -O2 the CPU side is built with: ppuc_load_code()
  # holds the whole module plus its laid-out image in CPU memory at
  # once, so a PPU program's compiled size has a ceiling of its own,
  # well below the PPU's own free memory.  See
  # libs/libppu/ppuc_load_code.c.  Later target_compile_options on
  # ${target}_objects win over this one.
  #
  # -fno-function-sections/-fno-data-sections undo what the toolchain
  # file asks for on the CPU side: this link is a relocatable one (-r),
  # so nothing is ever dropped, and a section per function only adds
  # headers to a module whose size is the thing being watched.
  target_compile_options(${objects} PRIVATE
    -Os -fno-function-sections -fno-data-sections)

  set(libraries "")
  foreach(library IN LISTS arg_LIBRARIES)
    list(APPEND libraries "-l${library}")
  endforeach()

  _uknc_script_command(ld_ppu "${UKNC_LD_PPU}")

  # Bare output names with the working directory set, so that the map
  # file lands beside the module rather than wherever the build was
  # started from.
  set(module "${CMAKE_CURRENT_BINARY_DIR}/${name}.ppu")
  add_custom_command(OUTPUT "${module}"
    COMMAND ${ld_ppu} -Map=${name}.ppu.map -o ${name}.ppu
            $<TARGET_OBJECTS:${objects}>
            -L${UKNC_SYSROOT}/lib ${libraries}
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    DEPENDS ${objects}
    COMMENT "Linking PPU module ${name}.ppu"
    COMMAND_EXPAND_LISTS VERBATIM)
  set(outputs "${module}")

  if(NOT arg_NO_ELF)
    # The same objects and the same layout, with the symbols and DWARF
    # an RT-11 object module has nowhere to keep.  Linked at address 0,
    # since where the module ends up is only decided when it is loaded.
    set(twin "${CMAKE_CURRENT_BINARY_DIR}/${name}.ppu.elf")
    add_custom_command(OUTPUT "${twin}"
      COMMAND ${ld_ppu} --elf -o ${name}.ppu.elf
              $<TARGET_OBJECTS:${objects}>
              -L${UKNC_SYSROOT}/lib ${libraries}
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
      DEPENDS ${objects}
      COMMENT "Linking PPU module ${name}.ppu.elf"
      COMMAND_EXPAND_LISTS VERBATIM)
    list(APPEND outputs "${twin}")
  endif()

  add_custom_target(${target} ALL DEPENDS ${outputs})
  # What uknc_add_disk puts on a disk when it is handed this target:
  # the module, and not its ELF twin -- that one is for the host.
  set_target_properties(${target} PROPERTIES UKNC_FILES "${module}")
endfunction()

# --------------------------------------------------------------------
# uknc_add_disk(<target> [NAME <name>] [BASE <image>] CONTENTS <items>...)
#
# A bootable disk image: a copy of the RT-11 system image with the given
# files written into it.  CONTENTS takes targets -- from
# uknc_add_program or uknc_add_ppu_module -- and plain files alike.
#
# A program being debugged does not need to be on the disk (gdb puts it
# straight into memory), but anything the program itself opens at run
# time does, a PPU module most of all.
function(uknc_add_disk target)
  cmake_parse_arguments(arg "" "NAME;BASE" "CONTENTS" ${ARGN})

  set(name "${target}")
  if(arg_NAME)
    set(name "${arg_NAME}")
  endif()
  set(base "${UKNC_SYSTEM_DISK}")
  if(arg_BASE)
    set(base "${arg_BASE}")
  endif()

  set(image "${CMAKE_CURRENT_BINARY_DIR}/${name}.dsk")
  # rt11dsk reads the file to add from the directory it is run in and
  # takes that same argument as the name to write into the catalogue, so
  # every file goes through one staging directory under a bare name.
  # Handed a path, it would quietly file it as the first six characters
  # of that path.
  set(staging "${CMAKE_CURRENT_BINARY_DIR}/${target}.files")
  # Now, rather than as the first command: a working directory has to
  # be there before anything can be run in it.
  file(MAKE_DIRECTORY "${staging}")

  set(commands
    COMMAND "${CMAKE_COMMAND}" -E rm -f "${image}"
    COMMAND "${CMAKE_COMMAND}" -E copy "${base}" "${image}")
  set(dependencies "${base}")

  foreach(item IN LISTS arg_CONTENTS)
    if(TARGET ${item})
      get_target_property(type ${item} TYPE)
      if(type STREQUAL "UTILITY")
        # A PPU module, or anything else that says what it produces.
        get_target_property(files ${item} UKNC_FILES)
        if(NOT files)
          message(FATAL_ERROR
            "uknc_add_disk(${target}): ${item} does not say what file it "
            "builds; name the file instead of the target.")
        endif()
      else()
        set(files "$<TARGET_FILE:${item}>")
      endif()
      list(APPEND dependencies ${item})
    else()
      get_filename_component(files "${item}" ABSOLUTE)
      list(APPEND dependencies "${files}")
      # A target's name was looked at when the target was made; a file
      # named here has not been looked at by anything.
      get_filename_component(stem "${item}" NAME_WE)
      get_filename_component(type "${item}" LAST_EXT)
      string(REPLACE "." "" type "${type}")
      _uknc_check_rt11_name("${stem}" "uknc_add_disk(${target})")
      string(LENGTH "${type}" length)
      if(length GREATER 3)
        message(WARNING
          "uknc_add_disk(${target}): \"${item}\" has a type RT-11 cannot "
          "keep -- three characters after the dot at most.")
      endif()
    endif()

    foreach(file IN LISTS files)
      if(file MATCHES "\\$<")
        set(filename "$<TARGET_FILE_NAME:${item}>")
      else()
        get_filename_component(filename "${file}" NAME)
      endif()
      list(APPEND commands
        COMMAND "${CMAKE_COMMAND}" -E copy "${file}" "${staging}/${filename}"
        COMMAND "${UKNC_RT11DSK}" a "${image}" "${filename}")
    endforeach()
  endforeach()

  add_custom_command(OUTPUT "${image}"
    ${commands}
    WORKING_DIRECTORY "${staging}"
    DEPENDS ${dependencies}
    COMMENT "Building disk image ${name}.dsk"
    COMMAND_EXPAND_LISTS VERBATIM)

  add_custom_target(${target} ALL DEPENDS "${image}")
  set_target_properties(${target} PROPERTIES UKNC_FILES "${image}")
endfunction()

# The command that runs a program on the machine, with every tool named
# outright.  uknc-run looks for gdb and rt11dsk on PATH when it is not
# told otherwise, and PATH is not what this build was configured with --
# a second toolchain installed beside this one would otherwise run the
# program the wrong one built.
function(_uknc_run_command var)
  _uknc_script_command(run "${UKNC_RUN}")
  set(${var}
    "${CMAKE_COMMAND}" -E env
    "UKNC_GDB=${UKNC_GDB}"
    "UKNC_RT11DSK=${UKNC_RT11DSK}"
    "UKNC_EMU=${UKNC_EMULATOR}"
    "UKNC_DISK=${UKNC_SYSTEM_DISK}"
    "UKNC_ROM=${UKNC_FIRMWARE}"
    -- ${run}
    PARENT_SCOPE)
endfunction()

# --------------------------------------------------------------------
# uknc_add_run(<target> [NAME <target name>])
#
# `cmake --build . --target run-<target>`: put the program on a copy of
# the system disk, run it on the machine, print what it printed, and
# report what it returned.  uknc-run does the work -- see the comments
# in it for how, and for what it does about a program that never
# finishes.
function(uknc_add_run target)
  cmake_parse_arguments(arg "" "NAME" "" ${ARGN})
  set(name "run-${target}")
  if(arg_NAME)
    set(name "${arg_NAME}")
  endif()
  _uknc_run_command(run)
  add_custom_target(${name}
    COMMAND ${run} "$<TARGET_FILE:${target}>"
    DEPENDS ${target}
    COMMENT "Running ${target} on the machine"
    USES_TERMINAL VERBATIM)
endfunction()

# --------------------------------------------------------------------
# uknc_add_test(<name> <target>)
#
# The same run as a ctest test: it passes when the program returns 0.
#
# A macro rather than a function so that the enable_testing() in it
# lands in the project's own directory, which is the only place it
# counts -- called inside a function it would apply to a scope that is
# gone by the time anything is generated, and ctest would find no tests
# at all.
macro(uknc_add_test name target)
  enable_testing()
  _uknc_run_command(_uknc_run)
  add_test(NAME "${name}" COMMAND ${_uknc_run} "$<TARGET_FILE:${target}>")
endmacro()
