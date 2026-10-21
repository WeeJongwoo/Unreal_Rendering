#include "MosaicShaders.h"

IMPLEMENT_GLOBAL_SHADER(FMosaicPS, "/CustomShaders/Mosaic.usf", "MosaicPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FMosaicDownsamplePS, "/CustomShaders/Mosaic.usf", "MosaicDownsamplePS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FMosaicReduceCS, "/CustomShaders/Mosaic.usf", "MosaicReduceCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FMosaicUpsamplePS, "/CustomShaders/Mosaic.usf", "MosaicUpsamplePS", SF_Pixel);
