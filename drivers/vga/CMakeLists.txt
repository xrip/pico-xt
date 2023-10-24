add_library(vga INTERFACE)

target_sources(vga INTERFACE
	${CMAKE_CURRENT_LIST_DIR}/vga_vmode.cpp
	${CMAKE_CURRENT_LIST_DIR}/vga.cpp
)

target_link_libraries(vga INTERFACE hardware_pio)

target_include_directories(vga INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
)

pico_generate_pio_header(vga
  ${CMAKE_CURRENT_LIST_DIR}/picovga.pio
)
