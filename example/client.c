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

int main(void) {
    jrpc_client_init(&my_client);
    my_client.debug_level = 1;

    jrpc_client_connect(&my_client, "127.0.0.1", PORT);

    jrpc_client_call(&my_client, "addTwoInts", 2, "1", "99");

    printf("done !!!");
	return 0;
}