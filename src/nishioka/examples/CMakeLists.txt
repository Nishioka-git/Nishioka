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
