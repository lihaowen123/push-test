#ifndef slic3r_Format_AssimpModel_hpp_
#define slic3r_Format_AssimpModel_hpp_

#include "../assimp/scene.h"

namespace Slic3r {

class TriangleMesh;
class ModelObject;

extern  bool load_assimp_model(const char* path, Model* model, bool& is_cancel, ImportStepProgressFn stepFn);

}; // namespace Slic3r

#endif 
