add_executable(strip-tbaa
  main.cc
)

llvm_map_components_to_libnames(LLVM_LIBS
  Core
  IRReader
  BitWriter
  Support
)

target_link_libraries(
  strip-tbaa
  PRIVATE
  ${LLVM_LIBS}
  nifty_core
)

install(
  TARGETS strip-tbaa
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
