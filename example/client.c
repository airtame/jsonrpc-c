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

    if (argc < 4) {
        printf("\n usage: %s IP number_addition number_to_add_1 number_to_add_2", argv[0]);
        return 1;
    }

    jrpc_client_connect(&my_client, argv[1], PORT);

    cJSON *result = jrpc_client_call(&my_client, "sayHello", 0);
    char *str_res = cJSON_Print(result);
    printf("Hello result: %s\n", str_res);
    free(str_res);
    cJSON_Delete(result);
    for (int i = 0;  i < atoi(argv[2]); i ++) {
        result = jrpc_client_call(&my_client, "addTwoInts", 2, argv[3], argv[4]);
        char *str_res = cJSON_Print(result);
        printf("[%d] Addition result: %s\n", i, str_res);
        free(str_res);
        cJSON_Delete(result);
    }
    jrpc_client_disconnect(&my_client);
	return 0;
}