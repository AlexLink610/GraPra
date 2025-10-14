# glob for source files
file(GLOB SOURCES "*.cpp")

set(TARGET bbm-server)

# executable target
add_executable(${TARGET} ${SOURCES})

# output in source
set_target_properties(${TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# handle dependencies
# Boost
if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(${TARGET} ${CMAKE_THREAD_LIBS_INIT})
    find_package(Boost REQUIRED COMPONENTS system)
    find_package(Boost COMPONENTS program_options REQUIRED)
    include_directories(${Boost_INCLUDE_DIRS})
    target_link_libraries(${TARGET} ${Boost_LIBRARIES})
else()
    find_package(Boost COMPONENTS program_options REQUIRED)
    include_directories(${Boost_INCLUDE_DIRS})
    target_link_libraries(${TARGET} ${Boost_LIBRARIES})

    # post build dll copy
    #copy cppgl dll
    add_custom_command(TARGET ${TARGET} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:cppgl> $<TARGET_FILE_DIR:${TARGET}>/$<TARGET_FILE_NAME:cppgl>)
    
    #get global cppgl dll location
    get_property(CPPGL_DEPENDACIES_DLL_LOCATION GLOBAL PROPERTY CPPGL_DEPENDACIES_DLL_BUILD_LOCATION)
    
    #THESE STRING ESCAPES NEEDS TO BE THERE, windows has different path structure
    # copy destination: current application folder
	STRING(REGEX REPLACE "/" "\\\\" copyDest \"${CMAKE_CURRENT_SOURCE_DIR}/\")
    # source dir: 
	STRING(REGEX REPLACE "/" "\\\\" copySource \"${CPPGL_DEPENDACIES_DLL_LOCATION}/*.dll\")
    # copy dlls
	add_custom_command(TARGET ${TARGET} POST_BUILD COMMAND COMMAND copy ${copySource}  ${copyDest})
    #glew needs extra handling
	STRING(REGEX REPLACE "/" "\\\\" copySourceGlew \"${CPPGL_DEPENDACIES_DLL_LOCATION}/subtrees/glew/bin/*.dll\")
	add_custom_command(TARGET ${TARGET} POST_BUILD COMMAND COMMAND copy ${copySourceGlew}  ${copyDest})
endif()
# messages
target_link_libraries(${TARGET} messages cppgl)
