#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "libslope.h"
#include "io.h"
#include "Options.h"

namespace slope {

using TransformMat = glm::mat4;

class Transform {
public:
    float angle;
    glm::vec3 axis,translation,scale;

    Transform() {
        angle = 0;
        scale = glm::vec3(1,1,1);
        axis = glm::vec3(0,0,1);
        translation = glm::vec3(0,0,0);
    }

    inline TransformMat getRotation() const {
        return glm::axisAngleMatrix(axis,angle);
    }

    inline mat getRotationEigen() const {
        return Eigen::AngleAxisd(angle,vec(axis.x,axis.y,axis.z)).toRotationMatrix();;
    }

    TransformMat getMatrix() const {
        TransformMat rslt = glm::scale(TransformMat(1.f),scale);
        rslt = getRotation()*rslt;
        rslt[3] = glm::vec4(translation,1);
        return rslt;
    }

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

    static Transform Translation(const vec& x) {
        Transform T;
        T.translation.x = x(0);
        T.translation.y = x(1);
        T.translation.z = x(2);
        return T;
    }

    static Transform AxisAngle(scalar th, vec x) {
        Transform T;
        T.angle = th;
        x.normalize();
        T.axis.x = x(0);
        T.axis.y = x(1);
        T.axis.z = x(2);
        return T;
    }

    static Transform Scale(const vec& x) {
        Transform T;
        T.scale.x = x(0);
        T.scale.y = x(1);
        T.scale.z = x(2);
        return T;
    }

    static Transform Scale(scalar x) {
        Transform T;
        T.scale.x = x;
        T.scale.y = x;
        T.scale.z = x;
        return T;
    }

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

    static mat RotFromEulerAngles(const Eigen::Vector<float,3>& euler_angles) {
        mat R;
        R = Eigen::AngleAxisd(euler_angles(0),vec::UnitX())
          * Eigen::AngleAxisd(euler_angles(1),vec::UnitY())
          * Eigen::AngleAxisd(euler_angles(2),vec::UnitZ());
        return R;
    }
};

class PersistentTransform {
    std::string label;

public:

    PersistentTransform() {}
    PersistentTransform(std::string l) : label(l) {}

    bool isActive() const {return label != "";}

    Transform readFromLabel() const{
        if (label == "")
            return Transform();
        std::ifstream f(slope::Options::ProjectViewsPath + label);
        if (!f) return Transform();
        vec s,t,axis;
        scalar angle;
        f >> s(0) >> s(1) >> s(2);
        f >> t(0) >> t(1) >> t(2);
        f >> axis(0) >> axis(1) >> axis(2);
        f >> angle;
        return Transform::ScalePositionRotate(s,t,axis,angle);
    }

    std::string getLabel() const {return label;}

    void writeAtLabel(const Transform& T) const {
        if (label == "")
            return;
        std::ofstream f(slope::Options::ProjectViewsPath +label);
        if (!f) {
            std::cerr << "[WARNING] cannot write transform to " << label << std::endl;
            return;
        }
        f << T.scale.x << " " << T.scale.y << " " << T.scale.z << std::endl;
        f << T.translation.x << " " << T.translation.y << " " << T.translation.z << std::endl;
        f << T.axis.x << " " << T.axis.y << " " << T.axis.z << std::endl;
        f << T.angle << std::endl;
    }

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
