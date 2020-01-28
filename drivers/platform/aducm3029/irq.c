/***************************************************************************//**
 *   @file   aducm3029/irq.c
 *   @brief  Implementation of External IRQ driver for ADuCM302x.
 *   @author Mihail Chindris (mihail.chindris@analog.com)
********************************************************************************
 * Copyright 2019(c) Analog Devices, Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  - Neither the name of Analog Devices, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *  - The use of this software may or may not infringe the patent rights
 *    of one or more patent holders.  This license does not release you
 *    from the requirement that you obtain separate licenses from these
 *    patent holders to use this software.
 *  - Use of the software either in source or binary form, must be run
 *    on or directly connected to an Analog Devices Inc. component.
 *
 * THIS SOFTWARE IS PROVIDED BY ANALOG DEVICES "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, NON-INFRINGEMENT,
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ANALOG DEVICES BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, INTELLECTUAL PROPERTY RIGHTS, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

/******************************************************************************/
/************************* Include Files **************************************/
/******************************************************************************/

#include "irq.h"
#include "irq_extra.h"
#include "gpio.h"
#include <stdlib.h>

/******************************************************************************/
/********************** Macros and Constants Definitions **********************/
/******************************************************************************/

/** Get the number resulting by shifting 1 with n positions */
#define BIT(n)		(1 << (n))
/** The number of the first external interrupt, used by NVIC */
#define BASE_XINT_NB	(XINT_EVT0_IRQn)

/** Map the interrupt ID to the ADI_XINT_EVENT associated event */
static const uint32_t id_map_event[NB_EXT_INTERRUPTS] = {
	ADI_XINT_EVENT_INT0, // ID 0
	ADI_XINT_EVENT_INT1, // ID 1
	ADI_XINT_EVENT_INT2, // ID 2
	ADI_XINT_EVENT_INT3  // ID 3
};

/******************************************************************************/
/***************************** Global Variables *******************************/
/******************************************************************************/

/** Bitmap with initialized interrupt */
static uint32_t		initialized;
/** Stores the enabled interrupts */
static uint32_t		enabled;
/** Memory needed by the ADI IRQ driver */
static uint8_t		irq_memory[ADI_XINT_MEMORY_SIZE];

/******************************************************************************/
/************************ Functions Definitions *******************************/
/******************************************************************************/

/**
 * @brief Calls the user callback
 * @param aducm_desc - Descriptor where the user callback is stared
 * @param event - Event that generated the callback
 * @param arg - Unused
 */
static void internal_callbackc(void *aducm_desc, uint32_t event, void *arg)
{
	struct aducm_irq_desc *desc = aducm_desc;

	(void)arg;

	switch (event) {
	case ADI_XINT_EVENT_INT0:
	case ADI_XINT_EVENT_INT1:
	case ADI_XINT_EVENT_INT2:
	case ADI_XINT_EVENT_INT3:
		desc->irq_handler(desc->param);
		break;
	default:
		break;
	}
}

/**
 * @brief Initialized the IRQ descriptor with the configuration given by param.
 *
 * An instance for each \ref irq_desc.irq_id can be initialized. The maximum
 * number of instances if \ref NB_EXT_INTERRUPTS .
 * @param desc - Pointer where the configured instance is stored
 * @param param - Configuration information for the instance
 * @return \ref SUCCESS in case of success, \ref FAILURE otherwise.
 */
int32_t irq_ctrl_init(struct irq_desc **desc,
		      const struct irq_init_param *param)
{
	struct aducm_irq_desc		*aducm_desc;

	if (!desc || !param || param->irq_id >= NB_EXT_INTERRUPTS ||
	    (initialized & BIT(param->irq_id)))
		return FAILURE;

	if (!(*desc = calloc(1, sizeof(**desc))))
		return FAILURE;
	if (!(aducm_desc = calloc(1, sizeof(*aducm_desc)))) {
		free(*desc);
		*desc = NULL;
		return FAILURE;
	}

	(*desc)->extra = aducm_desc;
	(*desc)->irq_id = param->irq_id;
	aducm_desc->mode = ((struct aducm_irq_init_param*)param->extra)->mode;
	aducm_desc->irq_handler = NULL;

	if (!initialized)
		adi_xint_Init(irq_memory, ADI_XINT_MEMORY_SIZE);
	struct gpio_init_param gpio_param = {
		.number = id_map_gpio[param->irq_id]
	};
	if (SUCCESS != gpio_get(&aducm_desc->gpio_desc, &gpio_param))
		goto failure;

	initialized |= BIT(param->irq_id);
	return SUCCESS;

failure:
	free(aducm_desc);
	free(*desc);
	*desc = NULL;
	return FAILURE;
}

/**
 * @brief Free the resources allocated by \ref irq_ctrl_init()
 * @param desc - Interrupt descriptor.
 * @return \ref SUCCESS in case of success, \ref FAILURE otherwise.
 */
int32_t irq_ctrl_remove(struct irq_desc *desc)
{
	struct aducm_irq_desc *aducm_desc;

	if (!desc || !desc->extra || !(initialized & BIT(desc->irq_id)))
		return FAILURE;

	aducm_desc = desc->extra;
	if (SUCCESS != gpio_remove(aducm_desc->gpio_desc))
		return FAILURE;
	initialized &= ~BIT(desc->irq_id);
	if (initialized == 0)
		adi_xint_UnInit();
	free(desc->extra);
	free(desc);

	return SUCCESS;
}

/**
 * @brief Registers a generic IRQ handling function
 * @param desc - Interrupt descriptor.
 * @param irq_id - Unused (\ref irq_desc.irq_id is used)
 * @param irq_handler - Generic function to be registered
 * @param dev_instance - Parameter to be pased to irq_handler
 * @return \ref SUCCESS in case of success, \ref FAILURE otherwise.
 */
int32_t irq_register(struct irq_desc *desc, uint32_t irq_id,
		     void (*irq_handler)(void *data), void *dev_instance)
{
	struct aducm_irq_desc *aducm_desc;
	if (!desc || !desc->extra || !(initialized & BIT(desc->irq_id))
	    || !irq_handler)
		return FAILURE;

	(void)irq_id;

	aducm_desc = desc->extra;
	aducm_desc->irq_handler = irq_handler;
	aducm_desc->param = dev_instance;

	adi_xint_RegisterCallback(id_map_event[desc->irq_id],
				  internal_callbackc, aducm_desc);

	return SUCCESS;
}

/**
 * @brief Unregisters a generic IRQ handling function
 * @param desc - Interrupt descriptor.
 * @param irq_id - Unused (\ref irq_desc.irq_id is used)
 * @return \ref SUCCESS in case of success, \ref FAILURE otherwise.
 */
int32_t irq_unregister(struct irq_desc *desc, uint32_t irq_id)
{
	if (!desc || !desc->extra || !(initialized & BIT(desc->irq_id)))
		return FAILURE;
	(void)irq_id;

	irq_source_disable(desc, irq_id);
	adi_xint_RegisterCallback(id_map_event[desc->irq_id], NULL, 0);

	return SUCCESS;
}

/**
 * @brief Enable all previously enabled interrupts by \ref irq_source_enable().
 * @param desc - Interrupt descriptor.
 * @return \ref SUCCESS
 */
int32_t irq_global_enable(struct irq_desc *desc)
{
	(void)desc;

	for (uint32_t i = 0; i < NB_EXT_INTERRUPTS; i++) {
		if (enabled & BIT(i))
			NVIC_EnableIRQ(BASE_XINT_NB + i);
	}

	return SUCCESS;
}

/**
 * @brief Disable all external interrupts
 * @param desc - Interrupt descriptor.
 * @return \ref SUCCESS
 */
int32_t irq_global_disable(struct irq_desc *desc)
{
	(void)desc;

	for (uint32_t i = 0; i < NB_EXT_INTERRUPTS; i++) {
		if (enabled & BIT(i))
			NVIC_DisableIRQ(BASE_XINT_NB + i);
	}

	return SUCCESS;
}

/**
 * @brief Enable the interrupt
 * @param desc - Interrupt descriptor.
 * @param irq_id - Unused (\ref irq_desc.irq_id is used)
 * @return \ref SUCCESS in case of success, \ref FAILURE otherwise.
 */
int32_t irq_source_enable(struct irq_desc *desc, uint32_t irq_id)
{
	(void)irq_id;

	struct aducm_irq_desc *aducm_desc;

	if (!desc || !desc->extra || !(initialized & BIT(desc->irq_id)))
		return FAILURE;
	aducm_desc = desc->extra;
	if (aducm_desc->irq_handler == NULL)
		return FAILURE;

	gpio_direction_input(aducm_desc->gpio_desc);
	adi_xint_EnableIRQ(id_map_event[desc->irq_id], aducm_desc->mode);
	enabled |= BIT(desc->irq_id);

	return SUCCESS;
}

/**
 * @brief Disable the interrupt
 * @param desc - Interrupt descriptor.
 * @param irq_id - Unused (\ref irq_desc.irq_id is used)
 * @return \ref SUCCESS in case of success, \ref FAILURE otherwise.
 */
int32_t irq_source_disable(struct irq_desc *desc, uint32_t irq_id)
{
	(void)irq_id;
	if (!desc || !(initialized & BIT(desc->irq_id)))
		return FAILURE;
	adi_xint_DisableIRQ(id_map_event[desc->irq_id]);
	enabled &= ~BIT(desc->irq_id);

	return SUCCESS;
}
