#----------------------------------------------------------------
# Generated CMake target import file for configuration "DEBUG".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Crc32c::crc32c" for configuration "DEBUG"
set_property(TARGET Crc32c::crc32c APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(Crc32c::crc32c PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/libcrc32c.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS Crc32c::crc32c )
list(APPEND _IMPORT_CHECK_FILES_FOR_Crc32c::crc32c "${_IMPORT_PREFIX}/lib/libcrc32c.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
