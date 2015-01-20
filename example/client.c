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

    if (argc < 3) {
        printf("\n usage: %s IP number_1 number_2", argv[0]);
        return 1;
    }

    jrpc_client_connect(&my_client, argv[1], PORT);

    cJSON *result = jrpc_client_call(&my_client, "sayHello", 0);
    printf("Hello result: %s\n", cJSON_Print(result));
    result = jrpc_client_call(&my_client, "addTwoInts", 2, argv[2], argv[3]);
    printf("Addition result: %s\n", cJSON_Print(result));
	return 0;
}