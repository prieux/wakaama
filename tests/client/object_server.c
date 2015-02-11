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
 *    Julien Vermillard, Sierra Wireless
 *    Bosch Software Innovations GmbH - Please refer to git log
 *    
 *******************************************************************************/

/*
 *  Resources:
 *
 *          Name         | ID | Operations | Instances | Mandatory |  Type   |  Range  | Units |
 *  Short ID             |  0 |     R      |  Single   |    Yes    | Integer | 1-65535 |       |
 *  Lifetime             |  1 |    R/W     |  Single   |    Yes    | Integer |         |   s   |
 *  Default Min Period   |  2 |    R/W     |  Single   |    No     | Integer |         |   s   |
 *  Default Max Period   |  3 |    R/W     |  Single   |    No     | Integer |         |   s   |
 *  Disable              |  4 |     E      |  Single   |    No     |         |         |       |
 *  Disable Timeout      |  5 |    R/W     |  Single   |    No     | Integer |         |   s   |
 *  Notification Storing |  6 |    R/W     |  Single   |    Yes    | Boolean |         |       |
 *  Binding              |  7 |    R/W     |  Single   |    Yes    | String  |         |       |
 *  Registration Update  |  8 |     E      |  Single   |    Yes    |         |         |       |
 *
 */

#include "liblwm2m.h"
#include "internals.h"

#include <stdlib.h>
#include <string.h>


typedef struct _server_instance_
{
    struct _server_instance_ * next;   // matches lwm2m_list_t::next
    uint16_t    instanceId;            // matches lwm2m_list_t::id
    uint16_t    shortServerId;
    uint32_t    lifetime;
    bool        storing;
    char        binding[4];
} server_instance_t;

static uint8_t prv_get_value(lwm2m_tlv_t * tlvP,
                             server_instance_t * targetP)
{
    // There are no multiple instance ressources
    tlvP->type = LWM2M_TYPE_RESSOURCE;

    switch (tlvP->id)
    {
    case LWM2M_SERVER_SHORT_ID_ID:
        lwm2m_tlv_encode_int(targetP->shortServerId, tlvP);
        if (0 != tlvP->length) return COAP_205_CONTENT;
        else return COAP_500_INTERNAL_SERVER_ERROR;

    case LWM2M_SERVER_LIFETIME_ID:
        lwm2m_tlv_encode_int(targetP->lifetime, tlvP);
        if (0 != tlvP->length) return COAP_205_CONTENT;
        else return COAP_500_INTERNAL_SERVER_ERROR;

    case LWM2M_SERVER_MIN_PERIOD_ID:
        return COAP_404_NOT_FOUND;

    case LWM2M_SERVER_MAX_PERIOD_ID:
        return COAP_404_NOT_FOUND;

    case LWM2M_SERVER_DISABLE_ID:
        return COAP_405_METHOD_NOT_ALLOWED;

    case LWM2M_SERVER_TIMEOUT_ID:
        return COAP_404_NOT_FOUND;

    case LWM2M_SERVER_STORING_ID:
        lwm2m_tlv_encode_bool(targetP->storing, tlvP);
        if (0 != tlvP->length) return COAP_205_CONTENT;
        else return COAP_500_INTERNAL_SERVER_ERROR;

    case LWM2M_SERVER_BINDING_ID:
        tlvP->value = targetP->binding;
        tlvP->length = strlen(targetP->binding);
        tlvP->flags = LWM2M_TLV_FLAG_STATIC_DATA;
        return COAP_205_CONTENT;

    case LWM2M_SERVER_UPDATE_ID:
        return COAP_405_METHOD_NOT_ALLOWED;

    default:
        return COAP_404_NOT_FOUND;
    }
}

static uint8_t prv_server_read(uint16_t instanceId,
                               int * numDataP,
                               lwm2m_tlv_t ** dataArrayP,
                               lwm2m_object_t * objectP)
{
    server_instance_t * targetP;
    uint8_t result;
    int i;

    targetP = (server_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;

    // is the server asking for the full instance ?
    if (*numDataP == 0)
    {
        uint16_t resList[] = {LWM2M_SERVER_SHORT_ID_ID, LWM2M_SERVER_LIFETIME_ID, LWM2M_SERVER_STORING_ID, LWM2M_SERVER_BINDING_ID};
        int nbRes = sizeof(resList)/sizeof(uint16_t);

        *dataArrayP = lwm2m_tlv_new(nbRes);
        if (*dataArrayP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;
        *numDataP = nbRes;
        for (i = 0 ; i < nbRes ; i++)
        {
            (*dataArrayP)[i].id = resList[i];
        }
    }

    i = 0;
    do
    {
        result = prv_get_value((*dataArrayP) + i, targetP);
        i++;
    } while (i < *numDataP && result == COAP_205_CONTENT);

    return result;
}

static uint8_t prv_server_write(uint16_t instanceId,
                                int numData,
                                lwm2m_tlv_t * dataArray,
                                lwm2m_object_t * objectP,
                                bool bootstrapPending)
{
    server_instance_t * targetP;
    int i;
    uint8_t result;

    targetP = (server_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP) {
        // TODO: manage object creation in case of a bootstrap sequence
        return COAP_404_NOT_FOUND;
    }

    i = 0;
    do
    {
        switch (dataArray[i].id)
        {
        case LWM2M_SERVER_SHORT_ID_ID:
            if (bootstrapPending) {
                int64_t value;
                if (1 == lwm2m_tlv_decode_int(dataArray + i, &value)) {
                    if (value >= 0 && value <= 0xFFFFFFFF) {
                        targetP->shortServerId = value;
                        result = COAP_204_CHANGED;
                    }
                    else {
                        result = COAP_406_NOT_ACCEPTABLE;
                    }
                }
                else {
                    result = COAP_400_BAD_REQUEST;
                }
            }
            else {
                LOG("    >>>> server is not allowed to write short ID\r\n");
                result = COAP_405_METHOD_NOT_ALLOWED;
            }
            break;

        case LWM2M_SERVER_LIFETIME_ID:
        {
            int64_t value;

            if (1 == lwm2m_tlv_decode_int(dataArray + i, &value))
            {
                if (value >= 0 && value <= 0xFFFFFFFF)
                {
                    targetP->lifetime = value;
                    result = COAP_204_CHANGED;
                }
                else
                {
                    result = COAP_406_NOT_ACCEPTABLE;
                }
            }
            else
            {
                result = COAP_400_BAD_REQUEST;
            }
        }
        break;

        case LWM2M_SERVER_MIN_PERIOD_ID:
            result = COAP_404_NOT_FOUND;
            break;

        case LWM2M_SERVER_MAX_PERIOD_ID:
            result = COAP_404_NOT_FOUND;
            break;

        case LWM2M_SERVER_DISABLE_ID:
            result = COAP_405_METHOD_NOT_ALLOWED;
            break;

        case LWM2M_SERVER_TIMEOUT_ID:
            result = COAP_404_NOT_FOUND;
            break;

        case LWM2M_SERVER_STORING_ID:
        {
            bool value;

            if (1 == lwm2m_tlv_decode_bool(dataArray + i, &value))
            {
                targetP->storing = value;
                result = COAP_204_CHANGED;
            }
            else
            {
                result = COAP_400_BAD_REQUEST;
            }
        }
        break;

        case LWM2M_SERVER_BINDING_ID:
            if ((dataArray[i].length > 0 && dataArray[i].length <= 3)
             && (strncmp(dataArray[i].value, "U", dataArray[i].length) == 0
              || strncmp(dataArray[i].value, "UQ", dataArray[i].length) == 0
              || strncmp(dataArray[i].value, "S", dataArray[i].length) == 0
              || strncmp(dataArray[i].value, "SQ", dataArray[i].length) == 0
              || strncmp(dataArray[i].value, "US", dataArray[i].length) == 0
              || strncmp(dataArray[i].value, "UQS", dataArray[i].length) == 0))
            {
                strncpy(targetP->binding, dataArray[i].value, dataArray[i].length);
                result = COAP_204_CHANGED;
            }
            else
            {
                result = COAP_400_BAD_REQUEST;
            }
            break;

        case LWM2M_SERVER_UPDATE_ID:
            result = COAP_405_METHOD_NOT_ALLOWED;
            break;

        default:
            return COAP_404_NOT_FOUND;
        }
        i++;
    } while (i < numData && result == COAP_204_CHANGED);

    return result;
}

static uint8_t prv_server_execute(uint16_t instanceId,
                                  uint16_t resourceId,
                                  char * buffer,
                                  int length,
                                  lwm2m_object_t * objectP)

{
    server_instance_t * targetP;
    uint8_t result;
    int i;

    targetP = (server_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;

    switch (resourceId)
    {
    case LWM2M_SERVER_DISABLE_ID:
        return COAP_404_NOT_FOUND;
    case LWM2M_SERVER_UPDATE_ID:
        return COAP_501_NOT_IMPLEMENTED;
    default:
        return COAP_405_METHOD_NOT_ALLOWED;
    }
}

static uint8_t prv_server_delete(uint16_t id,
                                 lwm2m_object_t * objectP)
{
    server_instance_t * serverInstance;

    objectP->instanceList = lwm2m_list_remove(objectP->instanceList, id, (lwm2m_list_t **)&serverInstance);
    if (NULL == serverInstance) return COAP_404_NOT_FOUND;

    lwm2m_free(serverInstance);

    return COAP_202_DELETED;
}

static uint8_t prv_server_create(uint16_t instanceId,
                                 int numData,
                                 lwm2m_tlv_t * dataArray,
                                 lwm2m_object_t * objectP)
{
    server_instance_t * serverInstance;
    uint8_t result;

    serverInstance = (server_instance_t *)lwm2m_malloc(sizeof(server_instance_t));
    if (NULL == serverInstance) return COAP_500_INTERNAL_SERVER_ERROR;
    memset(serverInstance, 0, sizeof(server_instance_t));

    serverInstance->instanceId = instanceId;
    objectP->instanceList = LWM2M_LIST_ADD(objectP->instanceList, serverInstance);

    result = prv_server_write(instanceId, numData, dataArray, objectP, false);

    if (result != COAP_204_CHANGED)
    {
        (void)prv_server_delete(instanceId, objectP);
    }
    else
    {
        result = COAP_201_CREATED;
    }

    return result;
}

static void prv_server_close(lwm2m_object_t * objectP)
{
    while (objectP->instanceList != NULL)
    {
        server_instance_t * serverInstance;

        serverInstance = (server_instance_t *)objectP->instanceList;
        objectP->instanceList = objectP->instanceList->next;

        lwm2m_free(serverInstance);
    }
}

static lwm2m_object_t * prv_server_copy(lwm2m_object_t * objectP)
{
    lwm2m_object_t * objectCopy = (lwm2m_object_t *)lwm2m_malloc(sizeof(lwm2m_object_t));
    if (NULL != objectCopy) {
        memcpy(objectCopy, objectP, sizeof(lwm2m_object_t));
        objectCopy->instanceList = NULL;
        objectCopy->userData = NULL;
        server_instance_t * instance = (server_instance_t *)objectP->instanceList;
        server_instance_t * previousInstanceCopy = NULL;
        while (instance != NULL) {
            server_instance_t * instanceCopy = (server_instance_t *)lwm2m_malloc(sizeof(server_instance_t));
            if (NULL == instanceCopy) {
                lwm2m_free(objectCopy);
                return NULL;
            }
            memcpy(instanceCopy, instance, sizeof(server_instance_t));
            // not sure it's necessary:
            strcpy(instanceCopy->binding, instance->binding);
            instance = (server_instance_t *)instance->next;
            if (previousInstanceCopy == NULL) {
                objectCopy->instanceList = (lwm2m_list_t *)instanceCopy;
            }
            else {
                previousInstanceCopy->next = instanceCopy;
            }
            previousInstanceCopy = instanceCopy;
        }
    }
    return objectCopy;
}

static void prv_server_print(lwm2m_object_t * objectP)
{
#ifdef WITH_LOGS
    LOG("Server object: %x, instanceList: %x\r\n", objectP, objectP->instanceList);
    server_instance_t * serverInstance = (server_instance_t *)objectP->instanceList;
    while (serverInstance != NULL) {
        LOG("    instance: %x, instanceId: %u, shortServerId: %u, lifetime: %u, storing: %s, binding: %s\r\n",
                serverInstance, serverInstance->instanceId, serverInstance->shortServerId, serverInstance->lifetime,
                serverInstance->storing ? "true" : "false", serverInstance->binding);
        serverInstance = (server_instance_t *)serverInstance->next;
    }
#endif
}

lwm2m_object_t * get_server_object(int serverId, const char* binding,
                                   int lifetime, bool storing)
{
    lwm2m_object_t * serverObj;

    serverObj = (lwm2m_object_t *)lwm2m_malloc(sizeof(lwm2m_object_t));

    if (NULL != serverObj)
    {
        server_instance_t * serverInstance;

        memset(serverObj, 0, sizeof(lwm2m_object_t));

        serverObj->objID = 1;

        // Manually create an hardcoded server
        serverInstance = (server_instance_t *)lwm2m_malloc(sizeof(server_instance_t));
        if (NULL == serverInstance)
        {
            lwm2m_free(serverObj);
            return NULL;
        }

        memset(serverInstance, 0, sizeof(server_instance_t));
        serverInstance->instanceId = 0;
        serverInstance->shortServerId = serverId;
        serverInstance->lifetime = lifetime;
        serverInstance->storing = storing;
        memcpy (serverInstance->binding, binding, strlen(binding)+1);
        serverObj->instanceList = LWM2M_LIST_ADD(serverObj->instanceList, serverInstance);

        serverObj->readFunc = prv_server_read;
        serverObj->writeFunc = prv_server_write;
        serverObj->createFunc = prv_server_create;
        serverObj->deleteFunc = prv_server_delete;
        serverObj->executeFunc = prv_server_execute;
        serverObj->closeFunc = prv_server_close;
        serverObj->copyFunc = prv_server_copy;
        serverObj->printFunc = prv_server_print;
    }

    return serverObj;
}
