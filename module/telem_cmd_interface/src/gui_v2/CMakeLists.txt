if(WIN32)
	set(NPM_COMMAND "npm.cmd")
else()
	set(NPM_COMMAND "npm")
endif()
execute_process(COMMAND ${NPM_COMMAND} --version
               OUTPUT_VARIABLE NPM_VERSION
               OUTPUT_STRIP_TRAILING_WHITESPACE)
#minimum version is npm 9.6.7 https://nodejs.org/en/blog/release/v18.17.0
SET(NPM_MINIMUM_VERSION "9.6.7")

if(NOT NPM_VERSION)
	message(FATAL_ERROR "Cannot find npm!")
endif()

string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+$" NPM_VERSION_MATCH "${NPM_VERSION}")
if(NOT NPM_VERSION_MATCH)
	message(FATAL_ERROR "Cannot find npm (or bad version returned)!")
endif()
string(LENGTH ${NPM_VERSION_MATCH} NPM_VERSION_STRLEN)
if(${NPM_VERSION_STRLEN} LESS 5) #1.1.1 minimum length
	message(FATAL_ERROR "Cannot find npm (version string must be at least 5 characters)!")
endif()
if(${NPM_VERSION_MATCH} VERSION_LESS ${NPM_MINIMUM_VERSION})
	message(FATAL_ERROR "Installed npm version ${NPM_VERSION_MATCH} is less than minimum required version ${NPM_MINIMUM_VERSION}")
endif()
message("Found npm version ${NPM_VERSION_MATCH}")
message("${CMAKE_CURRENT_SOURCE_DIR}")

add_custom_target(build_react_gui ALL
    COMMAND npm install && npm run build
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
