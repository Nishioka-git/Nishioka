build_lib(
  LIBNAME nishioka
  SOURCE_FILES
    model/nishioka-header.cc
    model/data-payload-header.cc
    model/nishioka-stack.cc
    model/nishioka-nwk.cc
    model/nishioka-rpl-payload-header.cc
    model/nishioka-rpl-tables.cc
    model/nishioka-rpl-routing.cc
    model/nishioka-rpl-nwk.cc
    helper/nishioka-helper.cc
    helper/nishioka-stack-container.cc
    helper/nishioka-rpl-helper.cc
  HEADER_FILES
    model/nishioka-header.h
    model/data-payload-header.h
    model/nishioka-stack.h
    model/nishioka-nwk.h
    model/nishioka-rpl-fields.h
    model/nishioka-rpl-payload-header.h
    model/nishioka-rpl-tables.h
    model/nishioka-rpl-routing.h
    model/nishioka-rpl-nwk.h
    helper/nishioka-helper.h
    helper/nishioka-stack-container.h
    helper/nishioka-rpl-helper.h
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