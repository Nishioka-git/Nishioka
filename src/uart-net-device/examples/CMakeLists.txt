build_lib_example(
  NAME uart-example
  SOURCE_FILES uart-example.cc
  LIBRARIES_TO_LINK ${liblr-wpan}
)
