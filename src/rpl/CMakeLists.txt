build_lib(
  LIBNAME rpl
  SOURCE_FILES
    helper/rpl-helper.cc
    model/rpl.cc
  HEADER_FILES
    helper/rpl-helper.h
    model/rpl.h
  LIBRARIES_TO_LINK ${libinternet}
)
