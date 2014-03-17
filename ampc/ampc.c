/*  `ampc' AMP API Compiler */

#include <stdio.h>
#include <stdlib.h>

#include "ampc.h"


AMP_DLL  AMP_Compiler_T *ampc_new(void)
{
    return (AMP_Compiler_T *)NULL;
}

AMP_DLL  void            ampc_free(AMP_Compiler_T *c)
{
}

AMP_DLL  amp_status_t    ampc_load_schema_file(char *in_pth)
{
    return 0;
}

AMP_DLL  amp_status_t    ampc_compile_header_file(char *out_pth, ampc_options_t *options)
{
    return 0;
}

