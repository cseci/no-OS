/***************************************************************************//**
 *   @file   iio_axi_adc_app.c
 *   @brief  Implementation of iio_axi_adc_app.
 *   This application instantiates iio_axi_adc and iio_axi_dac devices, for
 *   reading/writing and parameterization.
 *   @author Cristian Pop (cristian.pop@analog.com)
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
/***************************** Include Files **********************************/
/******************************************************************************/

#include "error.h"
#include "iio.h"
#include "iio_axi_adc_app.h"
#include "parameters.h"
#include "xil_cache.h"

/******************************************************************************/
/************************ Functions Definitions *******************************/
/******************************************************************************/

/**
 * @brief Application for reading/writing and parameterization of
 * axi_adc device.
 * @param  desc - Application descriptor.
 * @param init - Application configuration structure.
 * @return NO_OS_SUCCESS in case of success, NO_OS_FAILURE otherwise.
 */
int32_t iio_axi_adc_app_init(struct iio_axi_adc_app_desc **desc,
			     struct iio_axi_adc_app_init_param *init)
{
	struct iio_interface_init_par iio_axi_adc_intf_par;
	struct iio_device *iio_axi_adc_device;
	struct iio_axi_adc_init_par iio_axi_adc_init_par;
	struct iio_axi_adc *iio_axi_adc_inst;

	int32_t status;

	if (!init)
		return NO_OS_FAILURE;

	if (!init->rx_adc || !init->rx_dmac)
		return NO_OS_FAILURE;

	iio_axi_adc_init_par = (struct iio_axi_adc_init_par) {
		.adc = init->rx_adc,
		.dmac = init->rx_dmac,
		.adc_ddr_base = ADC_DDR_BASEADDR,
		.dcache_invalidate_range = (void (*)(uint32_t,
						     uint32_t))Xil_DCacheInvalidateRange,
	};

	status = iio_axi_adc_init(&iio_axi_adc_inst, &iio_axi_adc_init_par);
	if(status < 0)
		return NO_OS_FAILURE;

	iio_axi_adc_device = iio_axi_adc_create_device(iio_axi_adc_inst->adc->name,
			     iio_axi_adc_inst->adc->num_channels);
	if (!iio_axi_adc_device)
		return NO_OS_FAILURE;

	iio_axi_adc_intf_par = (struct iio_interface_init_par) {
		.dev_name = iio_axi_adc_inst->adc->name,
		.dev_instance = iio_axi_adc_inst,
		.iio_device = iio_axi_adc_device,
		.get_xml = iio_axi_adc_get_xml,
		.transfer_dev_to_mem = iio_axi_adc_transfer_dev_to_mem,
		.transfer_mem_to_dev = NULL,
		.read_data = iio_axi_adc_read_dev,
		.write_data = NULL,
	};

	status = iio_register(&iio_axi_adc_intf_par);
	if(status < 0)
		return status;

	*desc = calloc(1, sizeof(struct iio_axi_adc_app_desc));
	if (!(*desc))
		return NO_OS_FAILURE;

	(*desc)->iio_axi_adc_inst = iio_axi_adc_inst;

	return NO_OS_SUCCESS;
}

/**
 * @brief Release resources.
 * @param desc - Application descriptor.
 * @return NO_OS_SUCCESS in case of success, NO_OS_FAILURE otherwise.
 */
int32_t iio_axi_adc_app_remove(struct iio_axi_adc_app_desc *desc)
{
	int32_t status;

	if (!desc)
		return NO_OS_FAILURE;

	status = iio_unregister(desc->iio_axi_adc_inst->adc->name);
	if(status < 0)
		return status;

	free(desc);

	return NO_OS_SUCCESS;
}
