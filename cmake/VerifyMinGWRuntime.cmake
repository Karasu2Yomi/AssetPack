# Inspect PE imports without depending on the developer machine's DLL search PATH.
if (NOT EXISTS "${EXECUTABLE}")
    message(FATAL_ERROR "Executable not found: ${EXECUTABLE}")
endif()
if (NOT EXISTS "${OBJDUMP}")
    message(FATAL_ERROR "MinGW objdump not found: ${OBJDUMP}")
endif()

execute_process(
    COMMAND "${OBJDUMP}" -p "${EXECUTABLE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE imports
    ERROR_VARIABLE error
)
if (NOT result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect ${EXECUTABLE}: ${error}")
endif()

string(TOLOWER "${imports}" imports)
string(REGEX MATCHALL "dll name:[ \t]*[^\r\n]+" dependencies "${imports}")
if (NOT dependencies)
    message(FATAL_ERROR "No Windows DLL import table found in ${EXECUTABLE}")
endif()
foreach(dependency IN LISTS dependencies)
    if (dependency MATCHES "libgcc_s[^ \t]*\\.dll|libstdc\\+\\+[^ \t]*\\.dll|libwinpthread[^ \t]*\\.dll")
        message(FATAL_ERROR "Non-portable executable still requires ${dependency}")
    endif()
endforeach()
message(STATUS "Verified: no external MinGW runtime DLLs required")
