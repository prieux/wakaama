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
 *    Fabien Fleutot - Please refer to git log
 *    Simon Bernard - Please refer to git log
 *    Toby Jaffey - Please refer to git log
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


lwm2m_context_t * lwm2m_init(lwm2m_connect_server_callback_t connectCallback,
                             lwm2m_buffer_send_callback_t bufferSendCallback,
                             void * userData)
{
    lwm2m_context_t * contextP;

    if (NULL == bufferSendCallback)
        return NULL;

#ifdef LWM2M_CLIENT_MODE
    if (NULL == connectCallback)
        return NULL;
#endif

    contextP = (lwm2m_context_t *)lwm2m_malloc(sizeof(lwm2m_context_t));
    if (NULL != contextP)
    {
        memset(contextP, 0, sizeof(lwm2m_context_t));
        contextP->connectCallback = connectCallback;
        contextP->bufferSendCallback = bufferSendCallback;
        contextP->userData = userData;
        srand(time(NULL));
        contextP->nextMID = rand();
    }

    return contextP;
}

void lwm2m_close(lwm2m_context_t * contextP)
{
    int i;

#ifdef LWM2M_CLIENT_MODE
    for (i = 0 ; i < contextP->numObject ; i++)
    {
        if (NULL != contextP->objectList[i]->closeFunc)
        {
            contextP->objectList[i]->closeFunc(contextP->objectList[i]);
        }
        lwm2m_free(contextP->objectList[i]);
    }

    while (NULL != contextP->serverList)
    {
        lwm2m_server_t * targetP;

        targetP = contextP->serverList;
        contextP->serverList = contextP->serverList->next;

        registration_deregister(contextP, targetP);

        if (NULL != targetP->location) lwm2m_free(targetP->location);
        lwm2m_free(targetP);
    }

    while (NULL != contextP->bootstrapServerList)
    {
        lwm2m_server_t * targetP;

        targetP = contextP->bootstrapServerList;
        contextP->bootstrapServerList = contextP->bootstrapServerList->next;

        lwm2m_free(targetP);
    }

    while (NULL != contextP->observedList)
    {
        lwm2m_observed_t * targetP;

        targetP = contextP->observedList;
        contextP->observedList = contextP->observedList->next;

        while (NULL != targetP->watcherList)
        {
            lwm2m_watcher_t * watcherP;

            watcherP = targetP->watcherList;
            targetP->watcherList = targetP->watcherList->next;
            lwm2m_free(watcherP);
        }
        lwm2m_free(targetP);
    }

   if (NULL != contextP->objectList)
    {
        lwm2m_free(contextP->objectList);
    }

    lwm2m_free(contextP->endpointName);
#endif

#ifdef LWM2M_SERVER_MODE
    while (NULL != contextP->clientList)
    {
        lwm2m_client_t * clientP;

        clientP = contextP->clientList;
        contextP->clientList = contextP->clientList->next;

        prv_freeClient(clientP);
    }
#endif

    while (NULL != contextP->transactionList)
    {
        lwm2m_transaction_t * transacP;

        transacP = contextP->transactionList;
        contextP->transactionList = contextP->transactionList->next;

        transaction_free(transacP);
    }

    lwm2m_free(contextP);
}

#ifdef LWM2M_CLIENT_MODE
int lwm2m_configure(lwm2m_context_t * contextP,
                    char * endpointName,
                    lwm2m_binding_t binding,
                    char * msisdn,
                    uint16_t numObject,
                    lwm2m_object_t * objectList[])
{
    int i;
    uint8_t found;

    // This API can be called only once for now
    if (contextP->endpointName != NULL) return COAP_400_BAD_REQUEST;

    if (endpointName == NULL) return COAP_400_BAD_REQUEST;
    if (numObject < 3) return COAP_400_BAD_REQUEST;
    // Check that mandatory objects are present
    found = 0;
    for (i = 0 ; i < numObject ; i++)
    {
        if (objectList[i]->objID == LWM2M_SECURITY_OBJECT_ID) found |= 0x01;
        if (objectList[i]->objID == LWM2M_SERVER_OBJECT_ID) found |= 0x02;
        if (objectList[i]->objID == LWM2M_DEVICE_OBJECT_ID) found |= 0x04;
    }
    if (found != 0x07) return COAP_400_BAD_REQUEST;

    switch (binding)
    {
    case BINDING_UNKNOWN:
        return COAP_400_BAD_REQUEST;
    case BINDING_U:
    case BINDING_UQ:
        break;
    case BINDING_S:
    case BINDING_SQ:
    case BINDING_US:
    case BINDING_UQS:
        if (msisdn == NULL) return COAP_400_BAD_REQUEST;
        break;
    default:
        return COAP_400_BAD_REQUEST;
    }

    contextP->endpointName = strdup(endpointName);
    if (contextP->endpointName == NULL)
    {
        return COAP_500_INTERNAL_SERVER_ERROR;
    }

    contextP->binding = binding;
    if (msisdn != NULL)
    {
        contextP->msisdn = strdup(msisdn);
        if (contextP->msisdn == NULL)
        {
            return COAP_500_INTERNAL_SERVER_ERROR;
        }
    }

    contextP->objectList = (lwm2m_object_t **)lwm2m_malloc(numObject * sizeof(lwm2m_object_t *));
    if (NULL != contextP->objectList)
    {
        memcpy(contextP->objectList, objectList, numObject * sizeof(lwm2m_object_t *));
        contextP->numObject = numObject;
    }
    else
    {
        lwm2m_free(contextP->endpointName);
        contextP->endpointName = NULL;
        return COAP_500_INTERNAL_SERVER_ERROR;
    }

    return COAP_NO_ERROR;
}

void lwm2m_backup_objects(lwm2m_context_t * contextP)
{
    uint16_t i;

    // clean previous backup
    lwm2m_free(contextP->objectListBackup);
    contextP->objectListBackup = (lwm2m_object_t **)lwm2m_malloc(contextP->numObject * sizeof(lwm2m_object_t *));
    for (i = 0; i < contextP->numObject; i++) {
        lwm2m_object_t * object = contextP->objectList[i];
        fprintf(stdout, "OID: %d, read: %x, write: %x, exec: %x, create: %x, delete: %x, close: %x, copy: %x\r\n",
                object->objID, object->readFunc, object->writeFunc, object->executeFunc, object->createFunc,
                object->deleteFunc, object->closeFunc, object->copyFunc);
        switch (object->objID)
        {
        case 0: // security object
            break;
        case 1: // server object
            break;
        case 3: // device object
            break;
        case 5: // firmware object
            break;
        case 6: // location object
            break;
        default:
            fprintf(stdout, "  data: %s\r\n", object->userData);
            break;
        }
        lwm2m_list_t * instance = object->instanceList;
        while (instance != NULL)
        {
            fprintf(stdout, "    IID: %d\r\n", instance->id);
            instance = instance->next;
        }
        if (NULL != object->copyFunc)
        {
            lwm2m_object_t * backup = object->copyFunc(object);
            fprintf(stdout, ">>>> BACKUP, OID: %d, read: %x, write: %x, exec: %x, create: %x, delete: %x, close: %x, copy: %x\r\n",
                    backup->objID, backup->readFunc, backup->writeFunc, backup->executeFunc, backup->createFunc,
                    backup->deleteFunc, backup->closeFunc, backup->copyFunc);
            if (NULL != backup->printFunc) {
                object->printFunc(backup);
            }
        }
    }
}

void lwm2m_restore_objects(lwm2m_context_t * contextP)
{
}
#endif


int lwm2m_step(lwm2m_context_t * contextP,
               struct timeval * timeoutP)
{
    lwm2m_transaction_t * transacP;
    struct timeval tv;
#ifdef LWM2M_SERVER_MODE
    lwm2m_client_t * clientP;
#endif

    if (0 != lwm2m_gettimeofday(&tv, NULL)) return COAP_500_INTERNAL_SERVER_ERROR;

    transacP = contextP->transactionList;
    while (transacP != NULL)
    {
        // transaction_send() may remove transaction from the linked list
        lwm2m_transaction_t * nextP = transacP->next;
        int removed = 0;

        if (transacP->retrans_time <= tv.tv_sec)
        {
            removed = transaction_send(contextP, transacP);
        }

        if (0 == removed)
        {
            time_t interval;

            if (transacP->retrans_time > tv.tv_sec)
            {
                interval = transacP->retrans_time - tv.tv_sec;
            }
            else
            {
                interval = 1;
            }

            if (timeoutP->tv_sec > interval)
            {
                timeoutP->tv_sec = interval;
            }
        }

        transacP = nextP;
    }
#ifdef LWM2M_CLIENT_MODE
    lwm2m_update_registrations(contextP, tv.tv_sec, timeoutP);
    if (contextP->bsState == BOOTSTRAP_REQUESTED)
    {
        contextP->bsState = BOOTSTRAP_INITIATED;
        lwm2m_bootstrap(contextP);
    }
#endif

#ifdef LWM2M_SERVER_MODE
    // monitor clients lifetime
    clientP = contextP->clientList;
    while (clientP != NULL)
    {
        lwm2m_client_t * nextP = clientP->next;

        if (clientP->endOfLife <= tv.tv_sec)
        {
            contextP->clientList = (lwm2m_client_t *)LWM2M_LIST_RM(contextP->clientList, clientP->internalID, NULL);
            if (contextP->monitorCallback != NULL)
            {
                contextP->monitorCallback(clientP->internalID, NULL, DELETED_2_02, NULL, 0, contextP->monitorUserData);
            }
            prv_freeClient(clientP);
        }
        else
        {
            time_t interval;

            interval = clientP->endOfLife - tv.tv_sec;

            if (timeoutP->tv_sec > interval)
            {
                timeoutP->tv_sec = interval;
            }
        }
        clientP = nextP;
    }
#endif

    return 0;
}
