#ifndef __PINS_H__
#define __PINS_H__

#define __PIN_COMBO(x,y) x##y

#define __TMC2130_PIN_SET(x) \
		__PIN_COMBO(x,_DIR_PIN), \
		__PIN_COMBO(x,_ENABLE_PIN),\
		__PIN_COMBO(x,_STEP_PIN), \
		__PIN_COMBO(x,_TMC2130_CS),\
		__PIN_COMBO(x,_TMC2130_DIAG),\

// Pin names. Just add yours here.
namespace PinNames {
	enum Pin{
		BEEPER,
		BTN_EN1,
		BTN_EN2,
		BTN_ENC,
		BTN_ARRAY,
		__TMC2130_PIN_SET(E0)
		E0_FAN,
		FAN_1_PIN,
		FAN_PIN,
		FINDA_PIN,
		HEATER_0_PIN,
		HEATER_1_PIN,
		HEATER_2_PIN,
		HEATER_BED_PIN,
		__TMC2130_PIN_SET(I)
		IR_SENSOR_PIN,
		KILL_PIN,
		LCD_BL_PIN,
		LCD_PINS_D4,
		LCD_PINS_D5,
		LCD_PINS_D6,
		LCD_PINS_D7,
		LCD_PINS_ENABLE,
		LCD_PINS_RS,
		LED_PIN,
		__TMC2130_PIN_SET(P)
		PS_ON_PIN,
		__TMC2130_PIN_SET(S)
		SDCARDDETECT,
		SDPOWER,
		SDSS,
		SHIFT_CLOCK,
		SHIFT_DATA,
		SHIFT_LATCH,
		SUICIDE_PIN,
		SWI2C_SCL,
		SWI2C_SDA,
		TACH_0,
		TACH_1,
		TEMP_0_PIN,
		TEMP_1_PIN,
		TEMP_2_PIN,
		TEMP_AMBIENT_PIN,
		TEMP_BED_PIN,
		TEMP_PINDA_PIN,
		UVLO_PIN,
		VOLT_BED_PIN,
		VOLT_IR_PIN,
		VOLT_PWR_PIN,
		W25X20CL_PIN_CS,
		X_MAX_PIN,
		X_MIN_PIN,
		__TMC2130_PIN_SET(X)
		Y_MAX_PIN,
		Y_MIN_PIN,
		__TMC2130_PIN_SET(Y)
		Z_MAX_PIN,
		Z_MIN_PIN,
		__TMC2130_PIN_SET(Z)
		PIN_COUNT
	};
};

#endif //__PINS_H__
