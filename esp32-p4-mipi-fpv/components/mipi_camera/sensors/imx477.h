#pragma once

#include "../include/mipi_camera.h"
#include "imx219.h"  // For sensor_ops_t

sensor_ops_t* imx477_get_ops(void);
