add_library(vga-nextgen INTERFACE)

target_sources(vga-nextgen INTERFACE
	${CMAKE_CURRENT_LIST_DIR}/vga.c
)


target_include_directories(vga-nextgen INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
)
