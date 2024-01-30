check_include_files(
  "boost/asio.hpp"
  ASIO_FILES
  LANGUAGE
  CXX
)

build_lib(
  LIBNAME uart-net-device
  SOURCE_FILES model/uart-net-device.cc
               model/uart-lr-wpan-mac.cc
  HEADER_FILES model/uart-net-device.h
               model/uart-lr-wpan-mac.h
  LIBRARIES_TO_LINK ${libboost}
                    ${liblrwpan}
)
