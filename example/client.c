/*
 * client.c
 *
 *  Created on: Jan 19, 2015
 *      Author: elisescu
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include "jsonrpc-c.h"

#define PORT 1234  // the port users will be connecting to

struct jrpc_client my_client;

int main(int argc, char *argv[]) {
    jrpc_client_init(&my_client);
    my_client.debug_level = 1;

    if (argc < 2) {
        printf("\n usage: %s number_1 number_2", argv[0]);
        return 1;
    }

    jrpc_client_connect(&my_client, "127.0.0.1", PORT);

    cJSON *result = jrpc_client_call(&my_client, "addTwoInts", 2, "3", "4");
    printf("%s\n", cJSON_Print(result));
    result = jrpc_client_call(&my_client, "addTwoInts", 2, argv[1], argv[2]);
    printf("%s\n", cJSON_Print(result));
	return 0;
}