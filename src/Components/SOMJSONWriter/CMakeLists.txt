# Include the directory itself as a path to include directories
SET(CMAKE_INCLUDE_CURRENT_DIR ON)

# Create a variable containing all .cpp files:
FILE(GLOB files *.cpp)

# Find required packages


# Create an executable file from sources:
ADD_LIBRARY(SOMJSONWriter SHARED ${files})

# Link external libraries
TARGET_LINK_LIBRARIES(SOMJSONWriter ${DisCODe_LIBRARIES} ${PCL_COMMON_LIBRARIES} ${PCL_IO_LIBRARIES})

INSTALL_COMPONENT(SOMJSONWriter)
