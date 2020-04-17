/*
	TMC2130.c

    Simulates a TMC2130 driver for virtualizing Marlin on simAVR.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "avr_ioport.h"
#include "sim_avr.h"
#include "avr_spi.h"
#include "avr_adc.h"
#include "TMC2130.h"
#include "stdbool.h"
#include "GL/glut.h"
//#define TRACE(_w) _w
#ifndef TRACE
#define TRACE(_w)
#endif


void tmc2130_draw_glut(tmc2130_t *this)
{
        if (!this->iStepsPerMM)
            return; // Motors not ready yet.
        float fEnd = this->iMaxPos/this->iStepsPerMM;
	    glBegin(GL_QUADS);
			glVertex3f(0,0,0);
			glVertex3f(350,0,0);
			glVertex3f(350,10,0);
			glVertex3f(0,10,0);
		glEnd();
        glColor3f(1,1,1);
        glPushMatrix();
            glTranslatef(3,7,0);
            glScalef(0.09,-0.05,0);
            glutStrokeCharacter(GLUT_STROKE_MONO_ROMAN,this->axis);
        glPopMatrix();
        glPushMatrix();
            glTranslatef(280,7,0);
            glScalef(0.09,-0.05,0);
            char pos[7];
            sprintf(pos,"%3.02f",this->fCurPos);
            for (int i=0; i<7; i++)
                glutStrokeCharacter(GLUT_STROKE_MONO_ROMAN,pos[i]);

        glPopMatrix();
		glPushMatrix();
			glTranslatef(20,0,0);
			glColor3f(1,0,0);
			glBegin(GL_QUADS);
				glVertex3f(0,2,0);
				glVertex3f(-2,2,0);
				glVertex3f(-2,8,0);
				glVertex3f(0,8,0);
			glEnd();
			glBegin(GL_QUADS);
				glVertex3f(fEnd,2,0);
				glVertex3f(fEnd+2,2,0);
				glVertex3f(fEnd+2,8,0);
				glVertex3f(fEnd,8,0);
			glEnd();
			glColor3f(0,1,1);
			glBegin(GL_QUADS);
				glVertex3f(this->fCurPos-0.5,2,0);
				glVertex3f(this->fCurPos+0.5,2,0);
				glVertex3f(this->fCurPos+0.5,8,0);
				glVertex3f(this->fCurPos-0.5,8,0);
			glEnd();
		glPopMatrix();
}

static void tmc2130_create_reply(tmc2130_t *this)
{
    if (!this->cmdOut.bitsIn.RW) // Last in was a read.
    {
        this->cmdOut.bitsOut.data = this->regs.raw[this->cmdOut.bitsIn.address];
    }
    // If the last was a write, the old data is left intact.

    // Set the status bits on the reply:
    this->cmdOut.bitsOut.driver_error = this->regs.defs.GSTAT.drv_err;
    this->cmdOut.bitsOut.reset_flag = this->regs.defs.GSTAT.reset;
    this->cmdOut.bitsOut.sg2 = this->regs.defs.DRV_STATUS.stallGuard;
    this->cmdOut.bitsOut.standstill = this->regs.defs.DRV_STATUS.stst;
    TRACE(printf("Reply built: %10lx\n",this->cmdOut.all));
}

// Called when a full command is ready to process. 
static void tmc2130_process_command(tmc2130_t *this)
{
    TRACE(printf("tmc2130 %c cmd: w: %x a: %02x  d: %08x\n",this->axis, this->cmdIn.bitsIn.RW, this->cmdIn.bitsIn.address, this->cmdIn.bitsIn.data));
    if (this->cmdIn.bitsIn.RW)
    {
        this->regs.raw[this->cmdIn.bitsIn.address] = this->cmdIn.bitsIn.data;
        //printf("REG %c %02x set to: %010x\n", this->axis, this->cmdIn.bitsIn.address, this->cmdIn.bitsIn.data);
    }
    else
    {
        TRACE(printf("Read command on register: %02x\n", this->cmdIn.bitsIn.address));
    }
    tmc2130_create_reply(this);
    this->cmdOut = this->cmdIn;
}

/*
 * called when a SPI byte is received
 */
static void tmc2130_spi_in_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
    tmc2130_t* this = (tmc2130_t*)param;
    if (!this->flags.bits.selected)
        return;
    // Clock out a reply byte:
    uint8_t byte = this->cmdOut.bytes[4];
    this->cmdOut.all<<=8;
    avr_raise_irq(this->irq + IRQ_TMC2130_SPI_BYTE_OUT,byte);
    TRACE(printf("TMC2130 %c: Clocking out %02x\n",this->axis,byte));

    this->cmdIn.all<<=8; // Shift bits up
    this->cmdIn.bytes[0] = value;
    TRACE(printf("TMC2130 %c: byte received: %02x (%010lx)\n",this->axis,value, this->cmdIn.all));
    


}

static void tmc2130_check_diag(tmc2130_t *this)
{
    bool bDiag = this->regs.defs.DRV_STATUS.stallGuard && this->regs.defs.GCONF.diag0_stall;
    printf("Diag: %01x\n",bDiag);
    if (bDiag)
        avr_raise_irq(this->irq + IRQ_TMC2130_DIAG_OUT, bDiag^ this->regs.defs.GCONF.diag0_int_pushpull);
}

// Called when CSEL changes.
static void tmc2130_csel_in_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
    tmc2130_t* this = (tmc2130_t*)param;
	//TRACE(printf("TMC2130 %c: CSEL changed to %02x\n",this->axis,value));
    this->flags.bits.selected = (value==0); // NOTE: active low!
    if (value == 1) // Just finished a CSEL
        tmc2130_process_command(this);
}

// Called when DIR pin changes.
static void tmc2130_dir_in_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
    tmc2130_t* this = (tmc2130_t*)param;
	TRACE(printf("TMC2130 %c: DIR changed to %02x\n",this->axis,value));
    this->flags.bits.dir = value^this->flags.bits.inverted; // XOR
}

// Called when STEP is triggered.
static void tmc2130_step_in_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
    if (!value) return; // Only step on rising pulse
    tmc2130_t* this = (tmc2130_t*)param;
    if (!this->flags.bits.enable) return;
	TRACE(printf("TMC2130 %c: STEP changed to %02x\n",this->axis,value));
    if (this->flags.bits.dir)    
        this->iCurStep--;
    else
        this->iCurStep++;
    bool bStall = false;
    if (this->iCurStep==-1)
    {
        this->iCurStep = 0;
        bStall = true;
    }
    else if (this->iCurStep>this->iMaxPos)
    {
        this->iCurStep = this->iMaxPos;
        bStall = true;
    }
    if (this->iCurStep==200)
           avr_raise_irq(this->irq + IRQ_TMC2130_MIN_OUT, 1);
    else if (this->iCurStep == 201)
           avr_raise_irq(this->irq + IRQ_TMC2130_MIN_OUT, 0);

    this->fCurPos = (float)this->iCurStep/(float)this->iStepsPerMM;
    TRACE(printf("cur pos: %f (%u)\n",this->fCurPos,this->iCurStep));
    if (bStall)
        avr_raise_irq(this->irq + IRQ_TMC2130_DIAG_OUT, 1);
    else if (!bStall && this->regs.defs.DRV_STATUS.stallGuard)
          avr_raise_irq(this->irq + IRQ_TMC2130_DIAG_OUT, 0);
    this->regs.defs.DRV_STATUS.stallGuard = bStall;
    //tmc2130_check_diag(this);
}

// Called when DRV_EN is triggered.
static void tmc2130_enable_in_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
    tmc2130_t* this = (tmc2130_t*)param;
	TRACE(printf("TMC2130 %c: EN changed to %02x\n",this->axis,value));
    this->flags.bits.enable = value==0; // active low, i.e motors off when high.
}

static const char * irq_names[IRQ_TMC2130_COUNT] = {
		[IRQ_TMC2130_SPI_BYTE_IN] = "8<tmc2130.spi.in",
		[IRQ_TMC2130_SPI_BYTE_OUT] = "8>tmc2130.chain",
        [IRQ_TMC2130_SPI_COMMAND_IN] = "40<tmc2130.cmd",
        [IRQ_TMC2130_SPI_CSEL] = "tmc2130.csel",
        [IRQ_TMC2130_STEP_IN] = "tmc2130.step",
        [IRQ_TMC2130_DIR_IN] = "tmc2130.dir",
        [IRQ_TMC2130_ENABLE_IN] = "tmc2130.enable",
        [IRQ_TMC2130_DIAG_OUT] = "tmc2130.diag",
        [IRQ_TMC2130_DIAG_TRIGGER_IN] = "tmc2130.diagReq"
};

void
tmc2130_init(
		struct avr_t * avr,
		tmc2130_t *this,
        char axis, uint8_t iDiagPort) 
{
	this->irq = avr_alloc_irq(&avr->irq_pool, 0, IRQ_TMC2130_COUNT, irq_names);
    this->axis = axis;
    memset(&this->cmdIn, 0, sizeof(this->cmdIn));
    this->byteIndex = 4;
    this->fCurPos =25.0f; // start position.
    int iMaxMM = -1;
    switch (axis)
    {
        case 'Y':
            this->flags.bits.inverted = 1;
            this->iStepsPerMM = 100;
            iMaxMM = 220;
            break;
        case 'X':
            this->iStepsPerMM = 100;
            iMaxMM = 255;
            break;
        case 'Z': 
            this->iStepsPerMM = 400;
            iMaxMM = 210;
            break;
        case 'E':
            this->iCurStep = 0;
            this->iStepsPerMM = 490;
            break;
    }
    if (iMaxMM==-1)
        this->iMaxPos = -1;
    else
        this->iMaxPos = iMaxMM*this->iStepsPerMM;

    this->iCurStep = this->fCurPos*this->iStepsPerMM; // We track in "steps" to avoid the cumulative floating point error of adding fractions of a mm to each pos.

    this->regs.defs.DRV_STATUS.SG_RESULT = 250;

    // Just wire right up to the AVR SPI
    avr_connect_irq(avr_io_getirq(avr,AVR_IOCTL_SPI_GETIRQ(0),SPI_IRQ_OUTPUT),
		this->irq + IRQ_TMC2130_SPI_BYTE_IN);
    avr_connect_irq(this->irq + IRQ_TMC2130_SPI_BYTE_OUT,
        avr_io_getirq(avr,AVR_IOCTL_SPI_GETIRQ(0),SPI_IRQ_INPUT));
    
	avr_irq_register_notify(this->irq + IRQ_TMC2130_SPI_BYTE_IN, tmc2130_spi_in_hook, this);
    avr_irq_register_notify(this->irq + IRQ_TMC2130_SPI_CSEL, tmc2130_csel_in_hook, this);
    avr_irq_register_notify(this->irq + IRQ_TMC2130_DIR_IN, tmc2130_dir_in_hook, this);
    avr_irq_register_notify(this->irq + IRQ_TMC2130_STEP_IN, tmc2130_step_in_hook, this);
    avr_irq_register_notify(this->irq + IRQ_TMC2130_ENABLE_IN, tmc2130_enable_in_hook, this);
    avr_connect_irq(this->irq + IRQ_TMC2130_DIAG_OUT,
		avr_io_getirq(avr,AVR_IOCTL_IOPORT_GETIRQ('K'),iDiagPort));	

    avr_raise_irq(this->irq + IRQ_TMC2130_DIAG_OUT,0);

}

