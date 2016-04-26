#ifndef SOSD_CLOUD_MPI_H
#define SOSD_CLOUD_MPI_H

#include <mpi.h>

#include "sos.h"
#include "sos_debug.h"
#include "sosd.h"


/* For DAEMONs sending through cloud_sync: */
int   SOSD_cloud_init(int *argc, char ***argv);
int   SOSD_cloud_send(SOS_buffer *buffer);
void  SOSD_cloud_enqueue(SOS_buffer *buffer);
void  SOSD_cloud_fflush(void);
void  SOSD_cloud_shutdown_notice(void);
int   SOSD_cloud_finalize(void);

void* SOSD_THREAD_cloud_flush(void *params);

/* For the SOS_ROLE_DB reading from cloud_sync: */
void SOSD_cloud_listen_loop(void);


#endif
