/***************************************************************************//**
 *   @file   aducm3029/gpio.c
 *   @brief  Implementation of GPIO driver for ADuCM302x
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

#include "error.h"
#include "gpio.h"
#include <drivers/gpio/adi_gpio.h>
#include <stdlib.h>

/******************************************************************************/
/********************** Macros and Constants Definitions **********************/
/******************************************************************************/

/** Get GPIO pin from GPIO number */
#define PIN(nb)		(1u << ((nb) & 0x0F))
/** Get GPIO port from GPIO number */
#define PORT(nb)	(((nb) & 0xF0) >> 4)

/** Memory for GPIO device */
static uint8_t mem_gpio_handler[ADI_GPIO_MEMORY_SIZE];
/* Number of initialized devices */
static uint8_t nb_gpio = 0;

/******************************************************************************/
/************************ Functions Definitions *******************************/
/******************************************************************************/

/**
 * @brief Obtain the GPIO descriptor from the number specified in param
 * @param desc - Pointer to a structure were the descriptor will be stored.
 * @param param - Parameter describing the GPIO to be initialized
 * @return \ref NO_OS_SUCCESS in case of success, \ref NO_OS_FAILURE otherwise.
 */
int32_t gpio_get(struct gpio_desc **desc, const struct gpio_init_param *param)
{
	if (!desc || !param)
		return NO_OS_FAILURE;

	(*desc) = calloc(1, sizeof(**desc));
	if (!(*desc))
		return NO_OS_FAILURE;

	(*desc)->number = param->number;
	/* If this is the first GPIO initialize GPIO controller */
	if (nb_gpio == 0)
		if (NO_OS_SUCCESS != adi_gpio_Init(mem_gpio_handler,
					     ADI_GPIO_MEMORY_SIZE)) {
			free(*desc);
			*desc = NULL;
			return NO_OS_FAILURE;
		}

	/* Increment number of GPIOs */
	nb_gpio++;

	return NO_OS_SUCCESS;
}

/**
 * @brief Free the resources allocated by gpio_get().
 * @param desc - The GPIO descriptor.
 * @return \ref NO_OS_SUCCESS in case of success, \ref NO_OS_FAILURE otherwise.
 */
int32_t gpio_remove(struct gpio_desc *desc)
{
	if (!desc || !nb_gpio)
		return NO_OS_FAILURE;

	free(desc);
	/* Decrement number of GPIOs */
	nb_gpio--;
	/* If no more GPIOs free driver memory */
	if (nb_gpio == 0)
		return adi_gpio_UnInit();

	return NO_OS_SUCCESS;
}

/**
 * @brief Enable the input direction of the specified GPIO
 * @param desc - The GPIO descriptor.
 * @return \ref NO_OS_SUCCESS in case of success, \ref NO_OS_FAILURE otherwise.
 */
int32_t gpio_direction_input(struct gpio_desc *desc)
{
	if (!desc || !nb_gpio)
		return NO_OS_FAILURE;
	/* Enable input driver */
	if (ADI_GPIO_SUCCESS != adi_gpio_InputEnable(PORT(desc->number),
			PIN(desc->number), true))
		return NO_OS_FAILURE;

	return NO_OS_SUCCESS;
}

/**
 * @brief Enable the output direction of the specified GPIO and set the GPIO to
 * the specified value
 * @param desc - The GPIO descriptor.
 * @param value - The value. \ref GPIO_HIGH or \ref GPIO_LOW
 * @return \ref NO_OS_SUCCESS in case of success, \ref NO_OS_FAILURE otherwise.
 */
int32_t gpio_direction_output(struct gpio_desc *desc, uint8_t value)
{
	ADI_GPIO_RESULT ret;

	if (!desc || !nb_gpio)
		return NO_OS_FAILURE;

	/* Enable output driver */
	ret = adi_gpio_OutputEnable(PORT(desc->number), PIN(desc->number),
				    true);
	if (ret != ADI_GPIO_SUCCESS)
		return NO_OS_FAILURE;

	/* Initialize pin with a value */
	if (value == 1)
		ret = adi_gpio_SetHigh(PORT(desc->number), PIN(desc->number));
	else
		ret = adi_gpio_SetLow(PORT(desc->number), PIN(desc->number));
	if (ret != ADI_GPIO_SUCCESS)
		return NO_OS_FAILURE;

	return NO_OS_SUCCESS;
}

/**
 * @brief Get the direction of the specified GPIO.
 * @param desc - The GPIO descriptor.
 * @param direction - Variable where to store the direction. Will be set to \ref
 * GPIO_OUT or \ref GPIO_IN
 * @return \ref NO_OS_SUCCESS in case of success, \ref NO_OS_FAILURE otherwise.
 */
int32_t gpio_get_direction(struct gpio_desc *desc, uint8_t *direction)
{
	uint16_t pins;

	if (!desc || !nb_gpio)
		return NO_OS_FAILURE;

	if (ADI_GPIO_SUCCESS != adi_gpio_GetOutputEnable(PORT(desc->number),
			&pins))
		return NO_OS_FAILURE;
	if (pins & PIN(desc->number))
		*direction = GPIO_OUT;
	else
		*direction = GPIO_IN;

	return NO_OS_SUCCESS;
}

/**
 * @brief Set the value of the specified GPIO.
 * @param desc - The GPIO descriptor.
 * @param value - The value: GPIO_HIGH or GPIO_LOW
 * @return \ref NO_OS_SUCCESS in case of success, \ref NO_OS_FAILURE otherwise.
 */
int32_t gpio_set_value(struct gpio_desc *desc, uint8_t value)
{
	ADI_GPIO_RESULT ret;

	if (!desc || !nb_gpio)
		return NO_OS_FAILURE;

	if (value == GPIO_LOW)
		ret = adi_gpio_SetLow(PORT(desc->number), PIN(desc->number));
	else
		ret = adi_gpio_SetHigh(PORT(desc->number), PIN(desc->number));
	if (ret != ADI_GPIO_SUCCESS)
		return NO_OS_FAILURE;

	return NO_OS_SUCCESS;
}

/**
 * @brief Get the value of the specified GPIO.
 * @param desc - The GPIO descriptor.
 * @param value - Variable where to store the direction. Will be set to \ref
 * GPIO_HIGH or \ref GPIO_LOW
 * @return \ref NO_OS_SUCCESS in case of success, \ref NO_OS_FAILURE otherwise.
 */
int32_t gpio_get_value(struct gpio_desc *desc, uint8_t *value)
{
	uint16_t pins;

	if (!desc || !nb_gpio)
		return NO_OS_FAILURE;

	if (ADI_GPIO_SUCCESS != adi_gpio_GetData(PORT(desc->number),
			PIN(desc->number), &pins))
		return NO_OS_FAILURE;
	*value = !!pins;

	return NO_OS_SUCCESS;
}
