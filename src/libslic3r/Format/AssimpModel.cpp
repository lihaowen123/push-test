#include "../libslic3r.h"
#include "../Model.hpp"
#include "../TriangleMesh.hpp"
#include "AssimpModel.hpp"
#include <string>
#include "../assimp/Importer.hpp"
#include "../assimp/postprocess.h"


#ifdef _WIN32
#define DIR_SEPARATOR '\\'
#else
#define DIR_SEPARATOR '/'
#endif

namespace Slic3r
{

    struct AssimpCalculate
    {
        //��ȡ������
        Vec3f compute_triangle_normal(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2);

        //��ȡģ�ͼ�������
        Vec3f compute_global_center(const std::vector<Vec3f>& vertices);

        //ͨ������жϷ�����������������
        void Fix_Normal_Orientation(std::vector<Vec3f>& vertices, std::vector<Vec3i32>& faces);
    };

    Vec3f AssimpCalculate::compute_triangle_normal(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2)
    {
        Vec3f edge1 = v1 - v0;
        Vec3f edge2 = v2 - v0;
        Vec3f normal = edge1.cross(edge2);
        normal.normalize();
        return normal;
    }

    Vec3f AssimpCalculate::compute_global_center(const std::vector<Vec3f>& vertices)
    {
        Vec3f center = Vec3f::Zero();
        for (const auto& v : vertices)
        {
            center += v;
        }
        center /= vertices.size();
        return center;
    }

    void AssimpCalculate::Fix_Normal_Orientation(std::vector<Vec3f>& vertices, std::vector<Vec3i32>& faces)
    {
        if (vertices.empty() || faces.empty()) return;

        Vec3f global_center = compute_global_center(vertices);

        #pragma omp parallel for
        for (auto& face : faces)
        {
            const Vec3f& v0 = vertices[face(0)];
            const Vec3f& v1 = vertices[face(1)];
            const Vec3f& v2 = vertices[face(2)];
            Vec3f normal = compute_triangle_normal(v0, v1, v2);
            Vec3f face_center = (v0 + v1 + v2) / 3.0f;
            Vec3f dir = face_center - global_center;
            if (normal.dot(dir) < 0.0f)
            {
                std::swap(face(1), face(2)); // ��ת����˳��
            }
        }
    }

    bool load_assimp_model(const char* path, Model* model, bool& is_cancel, ImportStepProgressFn stepFn)
    {
        bool cb_cancel = false;
        if (stepFn)
        {
            stepFn(LOAD_STEP_STAGE_READ_FILE, 0, 1, cb_cancel);
            is_cancel = cb_cancel;
            if (cb_cancel)
            {
                return false;
            }
        }

        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path,
            aiProcess_Triangulate |             // ���ǻ�����
            aiProcess_FindInvalidData |         //�����޸���Ч������
            aiProcess_GenSmoothNormals |        // ����ƽ������
            aiProcess_ValidateDataStructure|    // ������ݽṹ��Ч��
            aiProcess_PreTransformVertices      //������ת��Ϊ��������ϵ
        );

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        {
            return false;
        }

        std::vector<Vec3f> vertices;
        std::vector<Vec3i32> faces;
        int offset = 0;

        // ֱ�ӱ�����������
        auto stage_unit = scene->mNumMeshes / LOAD_STEP_STAGE_UNIT_NUM + 1;
        for (unsigned i = 0; i < scene->mNumMeshes; i++)
        {
            if (stepFn)
            {
                if ((i % stage_unit) == 0)
                {
                    stepFn(LOAD_STEP_STAGE_GET_SOLID, i, scene->mNumMeshes, cb_cancel);
                    is_cancel = cb_cancel;
                }
                if (cb_cancel)
                {
                    return false;
                }
            }

            aiMesh* mesh = scene->mMeshes[i];
            if (!mesh) continue;

            // ��������
            for (unsigned j = 0; j < mesh->mNumVertices; j++)
            {
                aiVector3D& pos = mesh->mVertices[j];
                vertices.push_back(Vec3f(pos.x, pos.y, pos.z));
            }

            // ������
            for (unsigned m = 0; m < mesh->mNumFaces; m++)
            {
                aiFace& face = mesh->mFaces[m];
                if (face.mNumIndices != 3)
                    continue; // ȷ�����ǻ�

                faces.push_back(Vec3i32(face.mIndices[0] + offset, face.mIndices[1] + offset, face.mIndices[2] + offset));
            }

            offset += mesh->mNumVertices;
        }

        //���䴦��(3ds\dae)
        //���ڲ�ͬ�Ľ�ģ����в�ͬ����ϵ
        //�ᵼ�µ����ģ�ͷ�λҲ�᲻ͬ
        const char* dot = strrchr(path, '.');
        if (dot)
        {
            if ((strcmp(dot + 1, "3ds") == 0 || strcmp(dot + 1, "3DS") == 0) ||
               (strcmp(dot + 1, "dae") == 0 || strcmp(dot + 1, "dae") == 0))
            {
                aiMatrix4x4 rotationMatrix;
                rotationMatrix.FromEulerAnglesXYZ(ai_real(90.0f * M_PI / 180.0f), ai_real(0.0f), ai_real(0.0f));
                for (auto& vertex : vertices)
                {
                    aiVector3D pos(vertex.x(), vertex.y(), vertex.z());
                    aiVector3D rotatedPos = rotationMatrix * pos;
                    vertex.x() = rotatedPos.x;
                    vertex.y() = rotatedPos.y;
                    vertex.z() = rotatedPos.z;
                }
            }
        }

        // ������������
        TriangleMesh mesh(vertices, faces);

        // ��������Ƿ���Ч
        if (mesh.volume() <= 0.0)
        {
            // �޸����߷���
            AssimpCalculate AssCalc;
            AssCalc.Fix_Normal_Orientation(vertices, faces);
            mesh = TriangleMesh(vertices, faces);

            // �ٴμ�����
            //if (mesh.volume() <= 0.0)
            //{
            //    // �����Ȼ��Ч�����Է�ת������Ƭ�ķ���
            //    mesh.flip_triangles();
            //}
        }

        // ���ģ�Ͷ���
        std::string object_name;
        const char* last_slash = strrchr(path, DIR_SEPARATOR);
        object_name.assign((last_slash == nullptr) ? path : last_slash + 1);
        model->add_object(object_name.c_str(), path, std::move(mesh));

        return true;
    }


}; // namespace Slic3r
