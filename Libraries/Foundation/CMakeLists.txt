project(Foundation)

find_package(glm REQUIRED) # Find the GLM vector maths package.

add_library(${PROJECT_NAME} STATIC
		"API/Aurora/Foundation/BoundingBox.h"
		"API/Aurora/Foundation/Frustum.h"
		"API/Aurora/Foundation/Log.h"
		"API/Aurora/Foundation/Plane.h"
		"API/Aurora/Foundation/Timer.h"
		"API/Aurora/Foundation/Utilities.h"
		"API/Aurora/Foundation/Geometry.h"
		"Source/Geometry.cpp"
		"Source/Utilities.cpp"
		"Source/Log.cpp"
)

target_link_libraries(${PROJECT_NAME}
PRIVATE
    glm::glm
)

# Set custom ouput properties.
set_target_properties(${PROJECT_NAME} PROPERTIES
		FOLDER "Libraries"
		# TODO: set a custom output name for this target: OUTPUT_NAME "aurora_foundation"
		RUNTIME_OUTPUT_DIRECTORY "${RUNTIME_OUTPUT_DIR}"
		LIBRARY_OUTPUT_DIRECTORY "${LIBRARY_OUTPUT_DIR}"
		ARCHIVE_OUTPUT_DIRECTORY "${LIBRARY_OUTPUT_DIR}"
		PDB_OUTPUT_DIRECTORY "${RUNTIME_OUTPUT_DIR}"
)

# Add the Aurora folder a public include
target_include_directories(${PROJECT_NAME} PUBLIC "API")

target_compile_definitions(${PROJECT_NAME} PRIVATE ${DEFAULT_COMPILE_DEFINITIONS})