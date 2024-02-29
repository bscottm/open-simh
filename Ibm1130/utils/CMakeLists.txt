##~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=
## Additional IBM 1130 utilities. Note that these are packaged with the
## ibm1130software.zip file within the ibm1130.zip archive on ibm1130.org.
##
## Where do the resulting executables go?
## Not Windows: BIN/ibm1130-utils
## Windows: BIN/ibm1130-utils/<CONFIG>, where <CONFIG> is Debug, Release
##~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=

set(IBM1130_UTILS
      asm1130 bindump checkdisk disklist diskview mkboot punches
      viewdeck)

## Each utility is created separately -- it could be a foreach loop at the
## risk of reducing future flexibility.

add_executable(asm1130 asm1130.c util_io.c)
add_executable(bindump bindump.c util_io.c)
add_executable(checkdisk checkdisk.c util_io.c)
add_executable(disklist disklist.c util_io.c)
add_executable(diskview diskview.c util_io.c)
add_executable(mkboot mkboot.c util_io.c)
add_executable(punches punches.c util_io.c)
add_executable(viewdeck viewdeck.c util_io.c)

## Set the executable output directory:
foreach(util IN LISTS IBM1130_UTILS)
    set_target_properties(${util} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${SIMH_LEGACY_INSTALL}/ibm1130-utils/"
    )
endforeach()

## Meta-target to build all of the utilites:
add_custom_target(ibm1130-utils DEPENDS ${IBM1130_UTILS})
