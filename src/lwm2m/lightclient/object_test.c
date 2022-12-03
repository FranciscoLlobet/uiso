/*******************************************************************************
 *
 * Copyright (c) 2013, 2014 Intel Corporation and others.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v2.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v20.html
 * The Eclipse Distribution License is available at
 *    http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    David Navarro, Intel Corporation - initial API and implementation
 *    domedambrosio - Please refer to git log
 *    Fabien Fleutot - Please refer to git log
 *    Axel Lorente - Please refer to git log
 *    Achim Kraus, Bosch Software Innovations GmbH - Please refer to git log
 *    Pascal Rieux - Please refer to git log
 *    Ville Skyttä - Please refer to git log
 *    Scott Bertin, AMETEK, Inc. - Please refer to git log
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

#include "liblwm2m.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#define LWM2M_OBJECT_TEMPERATURE   3303

// Resource
#define RESOURCE_ID_SENSOR_VALUE       5700 // Sensor Value, float
#define RESOURCE_ID_MIN_MEASURED_VALUE 5601 // Optional
#define RESOURCE_ID_MAX_MEASURED_VALUE 5602
#define RESOURCE_ID_MIN_RANGE_VALUE    5603
#define RESOURCE_ID_MAX_RANGE_VALUE    5604
#define RESOURCE_ID_SENSOR_UNITS       5701
#define RESOURCE_ID_APPLICATION_TYPE   5750
#define RESOURCE_ID_TIMESTAMP          5518


static uint8_t prv_delete(lwm2m_context_t * contextP,
                          uint16_t id,
                          lwm2m_object_t * objectP);
static uint8_t prv_create(lwm2m_context_t * contextP,
                          uint16_t instanceId,
                          int numData,
                          lwm2m_data_t * dataArray,
                          lwm2m_object_t * objectP);

static void prv_output_buffer(uint8_t * buffer,
                              int length)
{
    int i;
    uint8_t array[16];

    i = 0;
    while (i < length)
    {
        int j;
        fprintf(stderr, "  ");

        memcpy(array, buffer+i, 16);

        for (j = 0 ; j < 16 && i+j < length; j++)
        {
            fprintf(stderr, "%02X ", array[j]);
        }
        while (j < 16)
        {
            fprintf(stderr, "   ");
            j++;
        }
        fprintf(stderr, "  ");
        for (j = 0 ; j < 16 && i+j < length; j++)
        {
            if (isprint(array[j]))
                fprintf(stderr, "%c ", array[j]);
            else
                fprintf(stderr, ". ");
        }
        fprintf(stderr, "\n");

        i += 16;
    }
}

/*
 * Multiple instance objects can use userdata to store data that will be shared between the different instances.
 * The lwm2m_object_t object structure - which represent every object of the liblwm2m as seen in the single instance
 * object - contain a chained list called instanceList with the object specific structure prv_instance_t:
 */
typedef struct _prv_instance_
{
    /*
     * The first two are mandatories and represent the pointer to the next instance and the ID of this one. The rest
     * is the instance scope user data (uint8_t test in this case)
     */
    struct _prv_instance_ * next;   // matches lwm2m_list_t::next
    uint16_t shortID;               // matches lwm2m_list_t::id
    uint8_t  test;
    double   dec;
    int16_t  sig;
} prv_instance_t;

static uint8_t prv_read(lwm2m_context_t * contextP,
                        uint16_t instanceId,
                        int * numDataP,
                        lwm2m_data_t ** dataArrayP,
                        lwm2m_object_t * objectP)
{
    /* Unused parameter */
    (void)contextP;

    uint8_t rVal = COAP_404_NOT_FOUND;

    if(instanceId != 0)
    {
    	return COAP_404_NOT_FOUND;
    }

    if(*numDataP == 0)
    {
    	*numDataP = 7;

    	*dataArrayP = lwm2m_data_new(6);
    	if(NULL == *dataArrayP)
    	{
    		return COAP_500_INTERNAL_SERVER_ERROR;
    	}

    	/* Populate data array */
    	(*dataArrayP)[0].id = RESOURCE_ID_SENSOR_VALUE;
    	(*dataArrayP)[1].id = RESOURCE_ID_MIN_MEASURED_VALUE;
    	(*dataArrayP)[2].id = RESOURCE_ID_MAX_MEASURED_VALUE;
    	(*dataArrayP)[3].id = RESOURCE_ID_MIN_RANGE_VALUE;
    	(*dataArrayP)[4].id = RESOURCE_ID_MAX_RANGE_VALUE;
    	(*dataArrayP)[5].id = RESOURCE_ID_TIMESTAMP;
    	(*dataArrayP)[6].id = RESOURCE_ID_SENSOR_UNITS;
    }

    for(int i = 0; i< *numDataP; i++)
    {
    	switch ( (*dataArrayP)[i].id )
    	{
    	case RESOURCE_ID_SENSOR_VALUE:
    		lwm2m_data_encode_float(20.0f, *dataArrayP + i);
    		rVal = COAP_205_CONTENT;
    		break;
    	case RESOURCE_ID_MIN_MEASURED_VALUE:
    		lwm2m_data_encode_float(20.0f, *dataArrayP + i);
    		rVal = COAP_205_CONTENT;
    		break;
    	case RESOURCE_ID_MAX_MEASURED_VALUE:
    		lwm2m_data_encode_float(20.0f, *dataArrayP + i);
    		rVal = COAP_205_CONTENT;
    		break;
    	case RESOURCE_ID_MIN_RANGE_VALUE:
    		lwm2m_data_encode_float(20.0f, *dataArrayP + i);
    		rVal = COAP_205_CONTENT;
    		break;
    	case RESOURCE_ID_MAX_RANGE_VALUE:
    		lwm2m_data_encode_float(20.0f, *dataArrayP + i);
    		rVal = COAP_205_CONTENT;
    		break;
    	case RESOURCE_ID_TIMESTAMP:
    		lwm2m_data_encode_int(lwm2m_gettime(), *dataArrayP + i);
    		rVal = COAP_205_CONTENT;
    		break;
    	case RESOURCE_ID_SENSOR_UNITS:
    		lwm2m_data_encode_string("Celsius", *dataArrayP + i);
    		rVal = COAP_205_CONTENT;
    		break;
    	default:
    		return COAP_404_NOT_FOUND;
    	}
    }

    return rVal;
}

static uint8_t prv_discover(lwm2m_context_t * contextP,
                            uint16_t instanceId,
                            int * numDataP,
                            lwm2m_data_t ** dataArrayP,
                            lwm2m_object_t * objectP)
{
    int i;

    /* Unused parameter */
    (void)contextP;

    // is the server asking for the full object ?
    if (*numDataP == 0)
    {
        *dataArrayP = lwm2m_data_new(4);
        if (*dataArrayP == NULL)
        {
        	return COAP_500_INTERNAL_SERVER_ERROR;
        }
        *numDataP = 4;
        (*dataArrayP)[0].id = 1;
        (*dataArrayP)[1].id = 2;
        (*dataArrayP)[2].id = 3;
        (*dataArrayP)[3].id = 4;
    }
    else
    {
        for (i = 0; i < *numDataP; i++)
        {
            switch ((*dataArrayP)[i].id)
            {
            case 1:
            case 2:
            case 3:
            case 4:
                break;
            default:
                return COAP_404_NOT_FOUND;
            }
        }
    }
    return COAP_205_CONTENT;
}

static uint8_t prv_write(lwm2m_context_t * contextP,
                         uint16_t instanceId,
                         int numData,
                         lwm2m_data_t * dataArray,
                         lwm2m_object_t * objectP,
                         lwm2m_write_type_t writeType)
{
    prv_instance_t * targetP;
    int i;

    targetP = (prv_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;

    if (writeType == LWM2M_WRITE_REPLACE_INSTANCE)
    {
        uint8_t result = prv_delete(contextP, instanceId, objectP);
        if (result == COAP_202_DELETED)
        {
            result = prv_create(contextP, instanceId, numData, dataArray, objectP);
            if (result == COAP_201_CREATED)
            {
                result = COAP_204_CHANGED;
            }
        }
        return result;
    }

    for (i = 0 ; i < numData ; i++)
    {
        /* No multiple instance resources */
        if (dataArray[i].type == LWM2M_TYPE_MULTIPLE_RESOURCE) return  COAP_404_NOT_FOUND;

        switch (dataArray[i].id)
        {
        case 1:
        {
            int64_t value;

            if (1 != lwm2m_data_decode_int(dataArray + i, &value) || value < 0 || value > 0xFF)
            {
                return COAP_400_BAD_REQUEST;
            }
            targetP->test = (uint8_t)value;
        }
        break;
        case 2:
            return COAP_405_METHOD_NOT_ALLOWED;
        case 3:
            if (1 != lwm2m_data_decode_float(dataArray + i, &(targetP->dec)))
            {
                return COAP_400_BAD_REQUEST;
            }
            break;
        case 4:
        {
            int64_t value;

            if (1 != lwm2m_data_decode_int(dataArray + i, &value) || value < INT16_MIN || value > INT16_MAX)
            {
                return COAP_400_BAD_REQUEST;
            }
            targetP->sig = (int16_t)value;
        }
        break;
        default:
            return COAP_404_NOT_FOUND;
        }
    }

    return COAP_204_CHANGED;
}

static uint8_t prv_delete(lwm2m_context_t * contextP,
                          uint16_t id,
                          lwm2m_object_t * objectP)
{
    prv_instance_t * targetP;

    /* Unused parameter */
    (void)contextP;

    objectP->instanceList = lwm2m_list_remove(objectP->instanceList, id, (lwm2m_list_t **)&targetP);
    if (NULL == targetP) return COAP_404_NOT_FOUND;

    lwm2m_free(targetP);

    return COAP_202_DELETED;
}

static uint8_t prv_create(lwm2m_context_t * contextP,
                          uint16_t instanceId,
                          int numData,
                          lwm2m_data_t * dataArray,
                          lwm2m_object_t * objectP)
{
    prv_instance_t * targetP;
    uint8_t result;


    targetP = (prv_instance_t *)lwm2m_malloc(sizeof(prv_instance_t));
    if (NULL == targetP) return COAP_500_INTERNAL_SERVER_ERROR;
    memset(targetP, 0, sizeof(prv_instance_t));

    targetP->shortID = instanceId;
    objectP->instanceList = LWM2M_LIST_ADD(objectP->instanceList, targetP);
    objectP->versionMajor = 1;
    objectP->versionMinor = 1;
    result = prv_write(contextP, instanceId, numData, dataArray, objectP, LWM2M_WRITE_REPLACE_RESOURCES);

    if (result != COAP_204_CHANGED)
    {
        (void)prv_delete(contextP, instanceId, objectP);
    }
    else
    {
        result = COAP_201_CREATED;
    }

    return result;
}

static uint8_t prv_exec(lwm2m_context_t * contextP,
                        uint16_t instanceId,
                        uint16_t resourceId,
                        uint8_t * buffer,
                        int length,
                        lwm2m_object_t * objectP)
{
    /* Unused parameter */
    (void)contextP;

    //if (NULL == lwm2m_list_find(objectP->instanceList, instanceId)) return COAP_404_NOT_FOUND;

    return COAP_405_METHOD_NOT_ALLOWED;
}

lwm2m_object_t * get_test_object(void)
{
    lwm2m_object_t * temp_object;

    temp_object = (lwm2m_object_t *)lwm2m_malloc(sizeof(lwm2m_object_t));

    if (NULL != temp_object)
    {
        int i;
        prv_instance_t * targetP;

        memset(temp_object, 0, sizeof(lwm2m_object_t));

        temp_object->objID = LWM2M_OBJECT_TEMPERATURE;

        temp_object->instanceList = (lwm2m_list_t *)lwm2m_malloc(sizeof(lwm2m_list_t));
        if (NULL != temp_object->instanceList)
        {
        	memset(temp_object->instanceList, 0, sizeof(lwm2m_list_t));
        }
        else
        {
        	lwm2m_free(temp_object);
        	return NULL;
        }

         /*
         * From a single instance object, two more functions are available.
         * - The first one (createFunc) create a new instance and filled it with the provided informations. If an ID is
         *   provided a check is done for verifying his disponibility, or a new one is generated.
         * - The other one (deleteFunc) delete an instance by removing it from the instance list (and freeing the memory
         *   allocated to it)
         */
        temp_object->readFunc = prv_read;
        temp_object->writeFunc = prv_write;
        temp_object->executeFunc = prv_exec;
        //temp_object->createFunc = prv_create;
        //temp_object->deleteFunc = prv_delete;
        temp_object->discoverFunc = prv_discover;
    }

    return temp_object;
}

void free_test_object(lwm2m_object_t * object)
{
    LWM2M_LIST_FREE(object->instanceList);
    if (object->userData != NULL)
    {
        lwm2m_free(object->userData);
        object->userData = NULL;
    }
    lwm2m_free(object);
}
