build_lib_example(
  NAME uart-example
  SOURCE_FILES uart-example.cc
  LIBRARIES_TO_LINK ${liblr-wpan}
)

build_lib_example(
  NAME uart-orphan-scan
  SOURCE_FILES uart-orphan-scan.cc
  LIBRARIES_TO_LINK ${liblr-wpan}
)
