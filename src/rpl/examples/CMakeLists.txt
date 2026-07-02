build_lib_example(
  NAME example-ping-lr-wpan-beacon-rpl
  SOURCE_FILES example-ping-lr-wpan-beacon-rpl.cc
  LIBRARIES_TO_LINK
    ${libnetwork}
    ${libsixlowpan}
    ${libinternet}
    ${liblr-wpan}
    ${libinternet-apps}
    ${librpl}
)
