#ifndef pyro_errors_h
#define pyro_errors_h

typedef enum {
    ERR_OK,
    ERR_ERROR,
    ERR_OUT_OF_MEMORY,
    ERR_OS_ERROR,
    ERR_ARGS_ERROR,
    ERR_ASSERTION_FAILED,
    ERR_TYPE_ERROR,
    ERR_NAME_ERROR,
    ERR_VALUE_ERROR,
    ERR_MODULE_NOT_FOUND,
    ERR_SYNTAX_ERROR,
} ErrorCode;

#endif
