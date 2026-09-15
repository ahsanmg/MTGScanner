include_guard()

######################################
##		ONNXRuntime	v1.22.2
######################################

include (FetchContent)
set(FETCHCONTENT_QUIET OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
set(FETCHCONTENT_GIT_PROGRESS ON)

set(MTGS_ORT_URL "" CACHE STRING "URL of the ONNX Runtime archive")
set(MTGS_ORT_URL_HASH "" CACHE STRING "SHA256 hash of the ONNX Runtime archive")

if (MTGS_ORT_URL STREQUAL "" OR MTGS_ORT_URL_HASH STREQUAL "")
	if (WIN32)
		set(MTGS_ORT_URL "https://github.com/ahsanmg/_deps/releases/download/2026.08/onnxruntime-v1.22.2-x64-windows.tar.gz")
		set(MTGS_ORT_URL_HASH "SHA256=b52da5b654f955fe00e44e86b3cf4e6821c42bc45447cf7ded238aeddb7f1a9b")
	elseif(LINUX)
		set(MTGS_ORT_URL "https://github.com/ahsanmg/_deps/releases/download/2026.08/onnxruntime-v1.22.2-x64-ubuntu.tar.gz")
		set(MTGS_ORT_URL_HASH "SHA256=6718342f184d4c2856a8738554096c90ae7169626b7b93175ee5e00347b2f4b8")
	endif()
endif()

if (onnxruntime_DIR AND NOT onnxruntime_DIR STREQUAL "")
	set(onnxruntime_ROOT "${onnxruntime_DIR}/../../../" CACHE STRING "Path to onnxruntime root directory")
elseif (onnxruntime_ROOT AND NOT onnxruntime_ROOT STREQUAL "")
	set(onnxruntime_DIR "${onnxruntime_ROOT}/lib/cmake/onnxruntime" CACHE STRING "Path to onnxruntime config files")
elseif (MTGS_ORT_URL AND MTGS_ORT_URL_HASH)
	    message(STATUS "Setting up onnxruntime from ${MTGS_ORT_URL}")

		FetchContent_Declare(onnxruntime
			URL ${MTGS_ORT_URL}
			URL_HASH ${MTGS_ORT_URL_HASH}
		)
	    FetchContent_MakeAvailable(onnxruntime)
		FetchContent_GetProperties(onnxruntime)

		if (onnxruntime_POPULATED)
			set(onnxruntime_DIR "${onnxruntime_SOURCE_DIR}/lib/cmake/onnxruntime" CACHE STRING "Path to onnxruntime config files")
			set(onnxruntime_ROOT ${onnxruntime_SOURCE_DIR} CACHE STRING "Path to onnxruntime root directory")
		endif()

		message(STATUS "Setup onnxruntime completed")
else()
	message(WARNING "Please set a valid path to onnxruntime_DIR.")
endif()
