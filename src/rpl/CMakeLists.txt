build_lib(
  LIBNAME rpl
  SOURCE_FILES
    helper/rpl-helper.cc
    model/rpl.cc
    model/rpl-header.cc
  HEADER_FILES
    helper/rpl-helper.h
    model/rpl.h
    model/rpl-header.h
  LIBRARIES_TO_LINK ${libinternet}
)
