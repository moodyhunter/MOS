

#include "mos-syslog.h"

#include <librpc/rpc_client.h>
#include <stdio.h>

int main()
{
    rpc_server_stub_t *logger = rpc_client_create(SYSLOGD_SERVICE_NAME);
    if (!logger)
    {
        fprintf(stderr, "failed to create rpc client\n");
        return 1;
    }

    syslogd_log(logger, "Hello from syslog-test!");
    syslogd_logc(logger, "ERROR", "Hello from syslog-test!");
}
