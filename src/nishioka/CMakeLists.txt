build_lib(
  LIBNAME nishioka
  SOURCE_FILES
    model/nishioka-header.cc
    model/data-payload-header.cc
  HEADER_FILES
    model/nishioka-header.h
    model/data-payload-header.h
  LIBRARIES_TO_LINK
    ${libspectrum}
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