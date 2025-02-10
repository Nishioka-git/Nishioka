build_lib_example(
  NAME uart-example
  SOURCE_FILES uart-example.cc
  LIBRARIES_TO_LINK ${liblr-wpan}
)

build_lib_example(
  NAME uartRealTime
  SOURCE_FILES uartRealTime.cc
  LIBRARIES_TO_LINK ${liblr-wpan}
)

build_lib_example(
  NAME uart-orphan-scan
  SOURCE_FILES uart-orphan-scan.cc
  LIBRARIES_TO_LINK ${liblr-wpan}
)

build_lib_example(
  NAME uart-sixlowpan-ping
  SOURCE_FILES uart-sixlowpan-ping.cc
  LIBRARIES_TO_LINK
    ${libnetwork}
    ${libsixlowpan}
    ${libinternet}
    ${liblr-wpan}
    ${libinternet-apps}
)

build_lib_example(
  NAME uart-zigbee-example
  SOURCE_FILES uart-zigbee-example.cc
  LIBRARIES_TO_LINK ${liblr-wpan}
                    ${libzigbee}
)
