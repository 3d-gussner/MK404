/*
	SPIPeripheral.h - Generalization helper for SPI-based peripherals.
    This header auto-wires the SPI and deals with some of the copypasta
    relating to checking CSEL and so on. You just need to have
    OnSPIIn and OnCSELIn overriden, as well as the SPI_BYTE_[*]/SPI_CSEL IRQs defined.

	Copyright 2020 VintagePC <https://github.com/vintagepc/>

 	This file is part of MK3SIM.

	MK3SIM is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	MK3SIM is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with MK3SIM.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef __SPI_PERIPHERAL_H__
#define __SPI_PERIPHERAL_H__

#include "BasePeripheral.h"
#include <avr_spi.h>

class SPIPeripheral: public BasePeripheral
{
    protected:
        virtual uint8_t OnSPIIn(struct avr_irq_t * irq, uint32_t value) = 0; 
        virtual void OnCSELIn(struct avr_irq_t * irq, uint32_t value) = 0;

        void _OnCSELIn(struct avr_irq_t * irq, uint32_t value)
        {
            m_bCSel = value;
            OnCSELIn(irq,value);
        };

        void SetSendReplyFlag(){m_bSendReply = true;}

        template<class C>
        void _OnSPIIn(struct avr_irq_t * irq, uint32_t value)
        {
            if (!m_bCSel)
            {
                m_bSendReply = false;
                uint8_t uiByteOut = OnSPIIn(irq, value);
                if (m_bSendReply)
                    RaiseIRQ(C::SPI_BYTE_OUT,uiByteOut);
            }
        }

        // Sets up the IRQs on "avr" for this class. Optional name override IRQNAMES.
        template<class C>
        void _Init(avr_t *avr, C *p, const char** IRQNAMES = nullptr) {
            BasePeripheral::_Init(avr,p, IRQNAMES);       

            RegisterNotify(C::SPI_BYTE_IN, MAKE_C_CALLBACK(SPIPeripheral,_OnSPIIn<C>), this);

            ConnectFrom(avr_io_getirq(avr,AVR_IOCTL_SPI_GETIRQ(0),SPI_IRQ_OUTPUT), C::SPI_BYTE_IN);
            ConnectTo(C::SPI_BYTE_OUT,avr_io_getirq(avr,AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_INPUT));
                     
            RegisterNotify(C::SPI_CSEL, MAKE_C_CALLBACK(SPIPeripheral,_OnCSELIn), this);
        }
        bool m_bCSel = true; // Chipselect, active low.
        bool m_bSendReply = false;
};

#endif