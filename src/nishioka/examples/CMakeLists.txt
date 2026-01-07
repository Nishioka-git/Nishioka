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
