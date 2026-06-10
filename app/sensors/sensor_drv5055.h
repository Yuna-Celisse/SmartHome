#ifndef SENSOR_DRV5055_H
#define SENSOR_DRV5055_H

#include "board_init.h"

int   DRV5055_Init(void);
float DRV5055_ReadMagneticFlux(void);

#endif /* SENSOR_DRV5055_H */
