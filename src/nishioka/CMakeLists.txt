build_lib(
  LIBNAME nishioka
  SOURCE_FILES
    model/nishioka-header.cc
    model/data-payload-header.cc
    model/nishioka-stack.cc
    model/nishioka-nwk.cc
    helper/nishioka-helper.cc
    helper/nishioka-stack-container.cc
  HEADER_FILES
    model/nishioka-header.h
    model/data-payload-header.h
    model/nishioka-stack.h
    model/nishioka-nwk.h
    helper/nishioka-helper.h
    helper/nishioka-stack-container.h
  LIBRARIES_TO_LINK
    ${libspectrum}
    ${liblr-wpan}
    ${libnetwork}
)

# Define module settings with NS3_ prefix for proper initialization
set(NS3_NISHIOKA ON)
set(ENABLE_NISHIOKA ON)

# Module dependencies
set(NS3_NISHIOKA_DEPENDENCIES
    spectrum
)

# Module description
set(NS3_NISHIOKA_DESCRIPTION "Nishioka header module for custom packet headers")