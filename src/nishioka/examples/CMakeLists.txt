build_lib_example(
  NAME simple-packet-send
  SOURCE_FILES simple-packet-send.cc
  LIBRARIES_TO_LINK
    ${libcore}
    ${libnetwork}
    ${libnishioka}
)