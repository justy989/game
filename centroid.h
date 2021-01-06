#pragma once

#include "coord.h"
#include "direction.h"

// Anthony has called these out as barycenter. This is debatable and I called them centroids first so ha

struct CentroidStart_t{
     Coord_t coord;
     Direction_t direction;
};

Coord_t find_centroid(CentroidStart_t a, CentroidStart_t b);
