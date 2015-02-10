/*******************************************************************************
 *
 * Copyright (c) 2013, 2014 Intel Corporation and others.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * The Eclipse Distribution License is available at
 *    http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    David Navarro, Intel Corporation - initial API and implementation
 *    domedambrosio - Please refer to git log
 *    Fabien Fleutot - Please refer to git log
 *    Simon Bernard - Please refer to git log
 *    Toby Jaffey - Please refer to git log
 *    Manuel Sangoi - Please refer to git log
 *    Julien Vermillard - Please refer to git log
 *    Bosch Software Innovations GmbH - Please refer to git log
 *
 *******************************************************************************/

/*
 Copyright (c) 2013, 2014 Intel Corporation

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

     * Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
     * Neither the name of Intel Corporation nor the names of its contributors
       may be used to endorse or promote products derived from this software
       without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 THE POSSIBILITY OF SUCH DAMAGE.

 David Navarro <david.navarro@intel.com>

*/

#include "internals.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef LWM2M_CLIENT_MODE

#define PRV_QUERY_BUFFER_LENGTH 200

static int prv_getBootstrapQuery(lwm2m_context_t * contextP,
                                 char * buffer,
                                 size_t length)
{
    int index = 0;

    index = snprintf(buffer, length, "?ep=%s", contextP->endpointName);
    if (index <= 1) return 0;

    return index;
}

static void prv_handleBootstrapReply(lwm2m_transaction_t * transacP, void * message)
{
    char code_as_string[5];
    lwm2m_server_t * targetP;
    
    LOG("    Handling bootstrap reply...\r\n");
    targetP = (lwm2m_server_t *)(transacP->peerP);
    LOG("    Server status: %d\r\n", targetP->status);
    coap_packet_t * packet = (coap_packet_t *)message;
    if (packet != NULL)
    {
        LOG("    Returned code: %u.%.2u\r\n", packet->code >> 5, packet->code & 0x1F);
    }
    else
    {
        LOG("    Bootstrap returned packet is null!\r\n");
    }
}

// start a device initiated bootstrap
int lwm2m_bootstrap(lwm2m_context_t * contextP) {
    char query[PRV_QUERY_BUFFER_LENGTH];
    int query_length = 0;

    lwm2m_transaction_t * transaction = NULL;

    query_length = prv_getBootstrapQuery(contextP, query, sizeof(query));
    if (query_length == 0) return INTERNAL_SERVER_ERROR_5_00;

    // find the first bootstrap server
    lwm2m_server_t * bootstrapServer = contextP->bootstrapServerList;
    if (bootstrapServer != NULL)
    {
        LOG("\r\nBootstrap server found\r\n");
        if (bootstrapServer->sessionH == NULL)
        {
            bootstrapServer->sessionH = contextP->connectCallback(bootstrapServer->shortID, contextP->userData);
        }
        if (bootstrapServer->sessionH != NULL)
        {
            LOG("bootstrap session starting...\r\n");
            transaction = transaction_new(COAP_POST, NULL, contextP->nextMID++, ENDPOINT_SERVER, (void *)bootstrapServer);
            if (transaction == NULL) return INTERNAL_SERVER_ERROR_5_00;

            coap_set_header_uri_path(transaction->message, "/"URI_BOOTSTRAP_SEGMENT);
            coap_set_header_uri_query(transaction->message, query);

            transaction->callback = prv_handleBootstrapReply;
            transaction->userData = (void *)bootstrapServer;

            contextP->transactionList = (lwm2m_transaction_t *)LWM2M_LIST_ADD(contextP->transactionList, transaction);
            if (transaction_send(contextP, transaction) == 0)
            {
                bootstrapServer->mid = transaction->mID;
                LOG("DI bootstrap requested to BS server\r\n");
            }
        }
        else
        {
            LOG("No bootstrap session handler found\r\n");
        }
    }
    return 0;
}

void handle_bootstrap(lwm2m_context_t * contextP,
                      coap_packet_t * message,
                      void * fromSessionH)
{
    if (COAP_204_CHANGED == message->code) {
        contextP->bsState = BOOTSTRAP_PENDING;
        LOG("    Received ACK/2.04, Bootstrap pending, waiting for DEL/PUT from BS server...\r\n");
        /*
         * TODO: starting here, we'll need to archive the object configuration in case of network or
         * botstrap server failure
         */
        lwm2m_backup_objects(contextP);
    }
    else
    {
        contextP->bsState = BOOTSTRAP_FAILED;
        LOG("    Bootstrap failed\r\n");
        lwm2m_restore_objects(contextP);
    }
}

#endif
