#ifndef pyro_errors_h
#define pyro_errors_h

typedef enum {
    ERR_OK,
    ERR_ERROR,
    ERR_OUT_OF_MEMORY,
    ERR_OS_ERROR,
    ERR_IO_ERROR,
    ERR_ARGS_ERROR,
    ERR_ASSERTION_FAILED,
    ERR_NAME_ERROR,
    ERR_TYPE_ERROR,
    ERR_DIV_BY_ZERO,
    ERR_MODULE_NOT_FOUND,
    ERR_SYNTAX_ERROR,
    ERR_OUT_OF_RANGE,
    ERR_VALUE_ERROR,
} ErrorCode;

typedef enum {
    SC_OK,
    SC_ERROR,
    SC_OUT_OF_MEMORY,
    SC_OS_ERROR,
    SC_IO_ERROR,
    SC_ARGS_ERROR,
    SC_ASSERTION_FAILED,
    SC_NAME_ERROR,
    SC_TYPE_ERROR,
    SC_DIV_BY_ZERO,
    SC_MODULE_NOT_FOUND,
    SC_SYNTAX_ERROR,
    SC_OUT_OF_RANGE,
    SC_VALUE_ERROR,
} PyroStatusCode;

#endif
