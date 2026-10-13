build_lib_example(
  NAME scenario
  SOURCE_FILES scenario.cc
  LIBRARIES_TO_LINK
    ${libnetwork}
    ${libmobility}
    ${libsixlowpan}
    ${libinternet}
    ${liblr-wpan}
    ${librpl}
    ${libnetanim}
)
