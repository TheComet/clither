#pragma once

#include "cstructures/config.h"

C_BEGIN

CSTRUCTURES_PUBLIC_API int
cs_init(void);

CSTRUCTURES_PUBLIC_API void
cs_deinit(void);

CSTRUCTURES_PUBLIC_API int
cs_threadlocal_init(void);

CSTRUCTURES_PUBLIC_API void
cs_threadlocal_deinit(void);

C_END
