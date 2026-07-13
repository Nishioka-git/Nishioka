# Boost.Asio を明示的に検出（check_include_files のキャッシュに依存しない）
# system は boost::system::error_code 等で使用
set(NS3_UART_NET_DEVICE ON)
find_package(Boost QUIET COMPONENTS system)
if(Boost_FOUND)
  # ヘッダーの存在確認（asio は Boost に含まれる）
  set(CMAKE_REQUIRED_INCLUDES ${Boost_INCLUDE_DIRS})
  check_include_files("boost/asio.hpp" HAVE_BOOST_ASIO LANGUAGE CXX)
  unset(CMAKE_REQUIRED_INCLUDES)
endif()

if(Boost_FOUND AND HAVE_BOOST_ASIO)
  add_definitions(-DHAVE_BOOST_ASIO)
  message(STATUS "Boost Asynchronous IO (ASIO) have been found.")
  set(libboost Boost::system)
else()
  message(
    STATUS
      "Boost asio are required for UART operations."
      " Skipping uart-net-device module build."
      " Install Boost and reconfigure to enable this module."
  )
  set(NS3_UART_NET_DEVICE OFF)
  return()
endif()

build_lib(
  LIBNAME uart-net-device
  SOURCE_FILES
    helper/uart-lr-wpan-helper.cc
    model/uart-lr-wpan-net-device.cc
    model/uart-lr-wpan-mac.cc
  HEADER_FILES
    helper/uart-lr-wpan-helper.h
    model/uart-lr-wpan-net-device.h
    model/uart-lr-wpan-mac.h
  LIBRARIES_TO_LINK ${libboost}
                    ${liblrwpan}
)

# uart-net-device に Boost のインクルードパスを明示的に付与
# （グローバル include_directories が伝わらない環境でもコンパイルできるようにする）
if(Boost_FOUND)
  target_include_directories(uart-net-device PRIVATE ${Boost_INCLUDE_DIRS})
endif()
