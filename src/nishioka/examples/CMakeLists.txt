if(NS3_UART_NET_DEVICE)
  build_lib_example(
    NAME multihop2
    SOURCE_FILES multihop2.cc
    LIBRARIES_TO_LINK
      ${libcore}
      ${libnetwork}
      ${liblr-wpan}
      ${libuart-net-device}
      ${libnishioka}
      ${libspectrum}
      ${libpropagation}
      ${libmobility}
      ${libantenna}
      ${libstats}
  )
endif()

build_lib_example(
  NAME nishioka-helper-example
  SOURCE_FILES nishioka-helper-example.cc
  LIBRARIES_TO_LINK
    ${libcore}
    ${libnetwork}
    ${liblr-wpan}
    ${libnishioka}
    ${libspectrum}
    ${libpropagation}
    ${libmobility}
)

build_lib_example(
  NAME nishioka-stack-simple
  SOURCE_FILES nishioka-stack-simple.cc
  LIBRARIES_TO_LINK
    ${libcore}
    ${libnetwork}
    ${liblr-wpan}
    ${libnishioka}
    ${libspectrum}
    ${libpropagation}
    ${libmobility}
)

build_lib_example(
  NAME multihop-static-lrwpan
  SOURCE_FILES multihop-static-lrwpan.cc
  LIBRARIES_TO_LINK
    ${libcore}
    ${libnetwork}
    ${liblr-wpan}
    ${libnishioka}
    ${libspectrum}
    ${libpropagation}
    ${libmobility}
)

build_lib_example(
  NAME nishioka-rpl-scaffold
  SOURCE_FILES nishioka-rpl-scaffold.cc
  LIBRARIES_TO_LINK
    ${libcore}
    ${libnetwork}
    ${liblr-wpan}
    ${libnishioka}
    ${libspectrum}
    ${libpropagation}
    ${libmobility}
)
