#ifndef AKINATOR_DUMP_H
#define AKINATOR_DUMP_H

#include "akinator.h"
#include "akinator_errors.h"

#define AKINATOR_DUMP(__akinator_pointer)\
    akinator_dump((__akinator_pointer),  \
                  __FILE__,              \
                  __LINE__,              \
                  __PRETTY_FUNCTION__);  \

akinator_error_t akinator_dump      (akinator_t *akinator,
                                     const char *caller_file,
                                     size_t      caller_line,
                                     const char *caller_func);

akinator_error_t akinator_dump_init (akinator_t *akinator);

#endif
