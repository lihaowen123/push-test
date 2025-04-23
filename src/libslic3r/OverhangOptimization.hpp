#ifndef OVERHANGOPTIMIZATION_HPP
#define OVERHANGOPTIMIZATION_HPP

#include "Model.hpp"
#include "admesh/stl.h"
#include <vector>
#include <utility>

namespace Slic3r {

class OverhangOptimization
{
public:
	//OverhangOptimization();
	void init(const ModelObject &object,float layer_height,const std::vector<double>& input_layer_height);
	std::vector<double> get_layer_height();
	std::vector<double> smooth_layer(const std::vector<double> r);

private:
	struct Face_Layer {
		std::pair<float, float> z_span;

		stl_vertex nor;
	};
	std::vector<Face_Layer> face_layer;
	std::vector<double >InputLayerHeight;
	float init_layer_height;
	float height_max = -10000.f;
	float height_min = 10000.f;
};
}

#endif