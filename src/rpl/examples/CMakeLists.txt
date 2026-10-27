build_lib_example(
  NAME scenario
  SOURCE_FILES scenario.cc
  LIBRARIES_TO_LINK
    ${libnetwork}
    ${libsixlowpan}
    ${libinternet}
    ${liblr-wpan}
    ${libinternet-apps}
    ${librpl}
)
