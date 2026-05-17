add_executable(
  nifty
  main.cc
)

llvm_map_components_to_libnames(LLVM_LIBS
  Core
  IRReader
  BitWriter
  Support
)

target_link_libraries(
  nifty
  PRIVATE
  ${LLVM_LIBS}
  nifty_core
)

install(
  TARGETS nifty
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
