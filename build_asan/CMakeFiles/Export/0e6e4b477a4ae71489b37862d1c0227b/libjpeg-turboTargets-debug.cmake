#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "libjpeg-turbo::jpeg" for configuration "Debug"
set_property(TARGET libjpeg-turbo::jpeg APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(libjpeg-turbo::jpeg PROPERTIES
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib64/libjpeg.so.62.4.0"
  IMPORTED_SONAME_DEBUG "libjpeg.so.62"
  )

list(APPEND _cmake_import_check_targets libjpeg-turbo::jpeg )
list(APPEND _cmake_import_check_files_for_libjpeg-turbo::jpeg "${_IMPORT_PREFIX}/lib64/libjpeg.so.62.4.0" )

# Import target "libjpeg-turbo::turbojpeg" for configuration "Debug"
set_property(TARGET libjpeg-turbo::turbojpeg APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(libjpeg-turbo::turbojpeg PROPERTIES
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib64/libturbojpeg.so.0.4.0"
  IMPORTED_SONAME_DEBUG "libturbojpeg.so.0"
  )

list(APPEND _cmake_import_check_targets libjpeg-turbo::turbojpeg )
list(APPEND _cmake_import_check_files_for_libjpeg-turbo::turbojpeg "${_IMPORT_PREFIX}/lib64/libturbojpeg.so.0.4.0" )

# Import target "libjpeg-turbo::turbojpeg-static" for configuration "Debug"
set_property(TARGET libjpeg-turbo::turbojpeg-static APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(libjpeg-turbo::turbojpeg-static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib64/libturbojpeg.a"
  )

list(APPEND _cmake_import_check_targets libjpeg-turbo::turbojpeg-static )
list(APPEND _cmake_import_check_files_for_libjpeg-turbo::turbojpeg-static "${_IMPORT_PREFIX}/lib64/libturbojpeg.a" )

# Import target "libjpeg-turbo::jpeg-static" for configuration "Debug"
set_property(TARGET libjpeg-turbo::jpeg-static APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(libjpeg-turbo::jpeg-static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib64/libjpeg.a"
  )

list(APPEND _cmake_import_check_targets libjpeg-turbo::jpeg-static )
list(APPEND _cmake_import_check_files_for_libjpeg-turbo::jpeg-static "${_IMPORT_PREFIX}/lib64/libjpeg.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
