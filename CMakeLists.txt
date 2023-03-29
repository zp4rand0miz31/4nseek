cmake_minimum_required (VERSION 2.8.9)
project (4nseek)

set(HEADERS 
  jpeg-parser.hpp 
  find-file.hpp 
  raw_extract.hpp
  4nseek.hpp)

set(SOURCES
  jpeg-parser.cpp
  raw_extract.cpp
  find-file.cpp)

add_definitions(--std=c++11 -g -Wall)

FIND_PACKAGE( Boost 1.40 COMPONENTS program_options system filesystem iostreams REQUIRED )
INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )

add_executable (find-in-file find-file.cpp ${SOURCES} ${HEADERS})
target_link_libraries(find-in-file ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_IOSTREAMS_LIBRARY})

