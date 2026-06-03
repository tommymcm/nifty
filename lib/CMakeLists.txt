add_library(
  nifty_core
  STATIC
  
  regions.cc
  strip.cc
  extract.cc
  diff.cc
  gumtree.cc
  print.cc
)

target_include_directories(nifty_core PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

install(TARGETS nifty_core
  EXPORT NiftyTargets
  ARCHIVE DESTINATION lib
)
