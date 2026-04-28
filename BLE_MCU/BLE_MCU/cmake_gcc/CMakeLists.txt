# Define minimal required version of CMake.
cmake_minimum_required(VERSION "3.25")

# Project definition
project(
	BLE_MCU
	VERSION 1.0
	LANGUAGES C CXX ASM
)

# Include the definition of the slc_BLE_MCU target,
# which contains the content of the SLC project
include(BLE_MCU.cmake)

add_executable(BLE_MCU
    # Add additional sources here
)

target_include_directories(BLE_MCU PUBLIC
    # Add additional include paths here
)

target_compile_definitions(BLE_MCU PUBLIC
    # Add additional macros here
)

target_compile_options(BLE_MCU PUBLIC
    # Set additional compiler flags here
)

target_link_options(BLE_MCU PUBLIC
    # Set additional linker flags here
)

# Link with the content defined in the SLC project
target_link_libraries(BLE_MCU PRIVATE
    slc
)

# Include managed project content if available
include(BLE_MCU_project.cmake OPTIONAL RESULT_VARIABLE managed_project)
if(managed_project)
    message(STATUS "Using managed project content from ${managed_project}")
endif()

# Force the gcc linker command
set_target_properties(BLE_MCU PROPERTIES LINKER_LANGUAGE C)

# Create .bin, .hex and .s37 artifacts after building the project
add_custom_command(TARGET BLE_MCU
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_SREC_CMD} "$<TARGET_FILE:BLE_MCU>" "$<TARGET_FILE_DIR:BLE_MCU>/$<TARGET_FILE_BASE_NAME:BLE_MCU>.s37"
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_IHEX_CMD} "$<TARGET_FILE:BLE_MCU>" "$<TARGET_FILE_DIR:BLE_MCU>/$<TARGET_FILE_BASE_NAME:BLE_MCU>.hex"
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_BIN_CMD}  "$<TARGET_FILE:BLE_MCU>" "$<TARGET_FILE_DIR:BLE_MCU>/$<TARGET_FILE_BASE_NAME:BLE_MCU>.bin" 
)

# Run post-build pipeline to perform additional post-processing
if(post_build_command)
add_custom_command(TARGET BLE_MCU
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
    COMMAND ${post_build_command}
)
endif()