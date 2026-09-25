#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "content/config/io.h"
#include "content/config/Options.h"
#include <optional>
#include <unordered_map>

namespace slope {

// 4x4 matrix of a transform.
using TransformMat = glm::mat4;


// Scale, then rotation around an axis, then translation.
class Transform {
public:
    // Rotation angle in radians around axis.
    float angle;
    glm::vec3 axis,translation,scale;

    // Sets the fields from a matrix made of scale, rotation and translation.
    void fromGLMMat4(const glm::mat4& matrix){
            translation = glm::vec3(matrix[3]);

            scale.x = glm::length(glm::vec3(matrix[0]));
            scale.y = glm::length(glm::vec3(matrix[1]));
            scale.z = glm::length(glm::vec3(matrix[2]));

            glm::mat3 rotationMatrix;
            rotationMatrix[0] = glm::vec3(matrix[0]) / scale.x;
            rotationMatrix[1] = glm::vec3(matrix[1]) / scale.y;
            rotationMatrix[2] = glm::vec3(matrix[2]) / scale.z;

            // A mirror is moved into the scale.
            if (glm::determinant(rotationMatrix) < 0.0f) {
                scale *= -1.0f;
                rotationMatrix *= -1.0f;
            }

            // Convert to axis and angle.
            glm::quat q = glm::quat_cast(rotationMatrix);
            angle = glm::angle(q);
            axis = glm::axis(q);
    }

    // Identity.
    Transform() {
        angle = 0;
        scale = glm::vec3(1,1,1);
        axis = glm::vec3(0,0,1);
        translation = glm::vec3(0,0,0);
    }

    // Rotation matrix.
    inline TransformMat getRotation() const {
        return glm::axisAngleMatrix(axis,angle);
    }

    // Rotation matrix, as an Eigen matrix.
    inline mat getRotationEigen() const {
        return Eigen::AngleAxisd(angle,vec(axis.x,axis.y,axis.z)).toRotationMatrix();
    }

    // Full matrix.
    TransformMat getMatrix() const {
        TransformMat rslt = glm::scale(TransformMat(1.f),scale);
        rslt = getRotation()*rslt;
        rslt[3] = glm::vec4(translation,1);
        return rslt;
    }

    // Blend of two transforms, giving T1 at t=0 and T2 at t=1.
    static Transform Interpolate(const Transform& T1,const Transform& T2,scalar t) {
        Transform T;
        auto R1 = T1.getRotation();
        auto R2 = T2.getRotation();
        auto R = glm::interpolate(R1,R2,(float)t);
        glm::axisAngle(R,T.axis,T.angle);
        T.translation = T1.translation*float(1-t) + T2.translation*float(t);
        T.scale = T1.scale*float(1-t) + T2.scale*float(t);
        return T;
    }

    // Pure translation.
    static Transform Translation(const vec& x) {
        Transform T;
        T.translation.x = x(0);
        T.translation.y = x(1);
        T.translation.z = x(2);
        return T;
    }

    // Pure rotation of angle th around the axis x.
    static Transform AxisAngle(scalar th, vec x) {
        Transform T;
        T.angle = th;
        x.normalize();
        T.axis.x = x(0);
        T.axis.y = x(1);
        T.axis.z = x(2);
        return T;
    }

    // Pure scale, with one factor per axis.
    static Transform Scale(const vec& x) {
        Transform T;
        T.scale.x = x(0);
        T.scale.y = x(1);
        T.scale.z = x(2);
        return T;
    }

    // Pure scale, the same on every axis.
    static Transform Scale(scalar x) {
        Transform T;
        T.scale.x = x;
        T.scale.y = x;
        T.scale.z = x;
        return T;
    }

    // Transform from a scale per axis, a position and a rotation given by axis and angle.
    static Transform ScalePositionRotate(const vec& s,const vec& p,vec axis,scalar th) {
        Transform T;
        T.scale.x = s(0);
        T.scale.y = s(1);
        T.scale.z = s(2);
        T.translation.x = p(0);
        T.translation.y = p(1);
        T.translation.z = p(2);
        T.angle = th;
        axis.normalize();
        T.axis.x = axis(0);
        T.axis.y = axis(1);
        T.axis.z = axis(2);
        return T;
    }

    // Same, with the rotation given as a matrix.
    static Transform ScalePositionRotate(const vec& s,const vec& p,const mat& R) {
        Transform T;
        T.scale.x = s(0);
        T.scale.y = s(1);
        T.scale.z = s(2);
        T.translation.x = p(0);
        T.translation.y = p(1);
        T.translation.z = p(2);

        Eigen::AngleAxisd aa(R);
        T.axis = glm::vec3(aa.axis()(0),aa.axis()(1),aa.axis()(2));
        T.angle = aa.angle();
        return T;
    }


    // Same, with one scale for all axes. The axis must be normalized.
    static Transform ScalePositionRotate(scalar s,const vec& p,const vec& axis,scalar th) {
        Transform T;
        T.scale.x = s;
        T.scale.y = s;
        T.scale.z = s;
        T.translation.x = p(0);
        T.translation.y = p(1);
        T.translation.z = p(2);
        T.angle = th;
        T.axis.x = axis(0);
        T.axis.y = axis(1);
        T.axis.z = axis(2);
        return T;
    }

    // Rotation matrix from the angles around x, y and z, applied in this order.
    static mat RotFromEulerAngles(const Eigen::Vector<float,3>& euler_angles) {
        mat R;
        R = Eigen::AngleAxisd(euler_angles(0),vec::UnitX())
          * Eigen::AngleAxisd(euler_angles(1),vec::UnitY())
          * Eigen::AngleAxisd(euler_angles(2),vec::UnitZ());
        return R;
    }
};

// A transform stored under a label, edited with a gizmo and saved in views/<label>.transform.
class PersistentTransform {
    std::string label;

    inline static std::unordered_map<std::string,Transform> session_cache;
    inline static std::set<std::string> dirty_labels;

    static std::string pathOf(const std::string& l) {
        return slope::Options::ProjectViewsPath + l + ".transform";
    }

    static bool readFile(const std::string& path,Transform& T) {
        std::ifstream f(path);
        if (!f) return false;
        vec s,t,axis;
        scalar angle;
        if (!(f >> s(0) >> s(1) >> s(2)
                >> t(0) >> t(1) >> t(2)
                >> axis(0) >> axis(1) >> axis(2)
                >> angle))
            return false;
        T = Transform::ScalePositionRotate(s,t,axis,angle);
        return true;
    }

public:

    // The gizmo is owned here because remove() in polyscope only unregisters it.
    // It is a shared_ptr because PersistentTransform is copied inside StateInSlide.
    std::shared_ptr<polyscope::TransformationGizmo> guizmo = nullptr;

    // Creates a gizmo that is removed when the last owner releases it.
    static std::shared_ptr<polyscope::TransformationGizmo> makeGuizmo(const std::string& name) {
        return std::shared_ptr<polyscope::TransformationGizmo>(
            new polyscope::TransformationGizmo(name),
            [](polyscope::TransformationGizmo* g) {
                if (g == nullptr)
                    return;
                g->setEnabled(false);
                g->remove();
                delete g;
            });
    }

    // Inactive, with no label.
    PersistentTransform() {}
    // Transform stored under the label l.
    PersistentTransform(std::string l) : label(l) {}

    // True when a label is set.
    bool isActive() const {return label != "";}

    // The label.
    std::string getLabel() const {return label;}

    // Stored transform, or nothing when the label was never placed.
    // A plane reads a missing transform as a billboard.
    std::optional<Transform> stored() const {
        if (label == "")
            return std::nullopt;
        auto it = session_cache.find(label);
        if (it != session_cache.end())
            return it->second;

        Transform T;
        if (!readFile(pathOf(label),T)
            && !readFile(slope::Options::ProjectViewsPath + label,T))
            return std::nullopt;
        session_cache[label] = T;
        return T;
    }

    // Stored transform, or the identity when there is none.
    Transform readFromLabel() const {
        auto T = stored();
        return T ? *T : Transform();
    }

    // Stores a new transform for the session. It is written to disk with Ctrl+S or when quitting.
    void writeAtLabel(const Transform& T) const {
        if (label == "")
            return;
        session_cache[label] = T;
        dirty_labels.insert(label);
    }

    // True when some transform was edited and not saved.
    static bool hasDirty() {return !dirty_labels.empty();}

    // Writes every edited transform to its file.
    static void saveAllDirty() {
        std::error_code ec;
        std::filesystem::create_directories(slope::Options::ProjectViewsPath, ec);
        std::set<std::string> unsaved;
        for (const auto& l : dirty_labels) {
            auto it = session_cache.find(l);
            if (it == session_cache.end())
                continue;
            const Transform& T = it->second;
            std::ofstream f(pathOf(l));
            if (!f) {
                spdlog::error("could not write {}", pathOf(l));
                unsaved.insert(l);
                continue;
            }
            f << T.scale.x << " " << T.scale.y << " " << T.scale.z << std::endl;
            f << T.translation.x << " " << T.translation.y << " " << T.translation.z << std::endl;
            f << T.axis.x << " " << T.axis.y << " " << T.axis.z << std::endl;
            f << T.angle << std::endl;
        }
        dirty_labels = std::move(unsaved);
    }

    // Draws position, rotation and scale fields. Returns true when T changed.
    bool ImGuiInterface(Transform& T) const {
        bool changed = false;

        ImGui::PushID(label.c_str());

        mat R = T.getRotationEigen();

        Eigen::Vector<float,3> euler_angles = R.eulerAngles(0,1,2).cast<float>();

        changed |= ImGui::DragFloat3("Position", glm::value_ptr(T.translation), 0.01f);
        changed |= ImGui::DragFloat3("Rotation (rad)", euler_angles.data(), 0.1f);
        changed |= ImGui::DragFloat3("Scale", glm::value_ptr(T.scale), 0.01f, 0.001f, 1000.0f);

        if (changed) {
            Eigen::AngleAxisd aa(Transform::RotFromEulerAngles(euler_angles));
            T.axis = glm::vec3(aa.axis()(0),aa.axis()(1),aa.axis()(2));
            T.angle = aa.angle();
        }

        ImGui::PopID();
        return changed;
    }
};

}


#endif // TRANSFORM_H
