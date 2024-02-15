/*******************************************************************************

Filename    :   XrMath.h
Content     :   Lightweight 3D math library for OpenXR.
Authors     :   Amanda M. Watson
License     :   Licensed under GPLv3 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#pragma once

#include "LogUtils.h"

#include <assert.h>
#include <cmath>

#ifndef MATH_FLOAT_PI
#define MATH_FLOAT_PI 3.14159265358979323846f
#endif

#ifndef MATH_FLOAT_TWOPI
#define MATH_FLOAT_TWOPI MATH_FLOAT_PI * 2.0f
#endif

static inline XrVector2f operator*(const XrVector2f& lhs, const float rhs) {
    return XrVector2f{lhs.x * rhs, lhs.y * rhs};
}

static inline XrVector3f operator+(const XrVector3f& lhs, const XrVector3f& rhs) {
    return XrVector3f{lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

static inline XrVector3f operator-(const XrVector3f& lhs, const XrVector3f& rhs) {
    return XrVector3f{lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

static inline XrVector3f operator*(const XrVector3f& lhs, const float rhs) {
    return XrVector3f{lhs.x * rhs, lhs.y * rhs, lhs.z * rhs};
}

static inline XrVector3f operator*(const float lhs, const XrVector3f& rhs) {
    return XrVector3f{lhs * rhs.x, lhs * rhs.y, lhs * rhs.z};
}

static inline XrQuaternionf operator*(const XrQuaternionf& lhs, const XrQuaternionf& rhs) {
    XrQuaternionf result;
    result.x = lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y;
    result.y = lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x;
    result.z = lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w;
    result.w = lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z;
    return result;
}

namespace XrMath {

static constexpr float kEpsilon = 0.00001f;

static inline float SafeRcpSqrt(const float x) {
    return (x >= std::numeric_limits<float>::min()) ? 1.0f / sqrtf(x)
                                                    : std::numeric_limits<float>::max();
}

class Vector3f {
public:
    static float LengthSq(const XrVector3f& v) { return v.x * v.x + v.y * v.y + v.z * v.z; }
    static float Length(const XrVector3f& v) { return sqrtf(LengthSq(v)); }

    static void Normalize(XrVector3f& v) {
        const float lengthRcp = SafeRcpSqrt(LengthSq(v));
        v.x *= lengthRcp;
        v.y *= lengthRcp;
        v.z *= lengthRcp;
    }

    static XrVector3f Normalized(const XrVector3f& v) {
        XrVector3f res = v;
        Normalize(res);
        return res;
    }

    static XrVector3f Cross(const XrVector3f& a, const XrVector3f& b) {
        return XrVector3f{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    }
};

class Matrixf
{
public:
    static void Identity(XrVector4f mat[4]) {
        mat[0] = {1.f, 0.f, 0.f, 0.f};
        mat[1] = {0.f, 1.f, 0.f, 0.f};
        mat[2] = {0.f, 0.f, 1.f, 0.f};
        mat[3] = {0.f, 0.f, 0.f, 1.f};
    }

    static XrVector4f XrVector4f_Multiply(const XrVector4f mat[4], const XrVector4f &v)
    {
        XrVector4f out;
        out.x = mat[0].x * v.x + mat[0].y * v.y + mat[0].z * v.z + mat[0].w * v.w;
        out.y = mat[1].x * v.x + mat[1].y * v.y + mat[1].z * v.z + mat[1].w * v.w;
        out.z = mat[2].x * v.x + mat[2].y * v.y + mat[2].z * v.z + mat[2].w * v.w;
        out.w = mat[3].x * v.x + mat[3].y * v.y + mat[3].z * v.z + mat[3].w * v.w;
        return out;
    }

    static XrVector3f XrVector3f_Multiply(const XrVector3f mat[3], const XrVector3f &v)
    {
        XrVector3f out;
        out.x = mat[0].x * v.x + mat[0].y * v.y + mat[0].z * v.z;
        out.y = mat[1].x * v.x + mat[1].y * v.y + mat[1].z * v.z;
        out.z = mat[2].x * v.x + mat[2].y * v.y + mat[2].z * v.z;
        return out;
    }

    // Returns a 3x3 minor of a 4x4 matrix.
    static float ToMinor(const float *matrix, int r0, int r1, int r2, int c0, int c1, int c2) {
        return matrix[4 * r0 + c0] *
               (matrix[4 * r1 + c1] * matrix[4 * r2 + c2] - matrix[4 * r2 + c1] * matrix[4 * r1 + c2]) -
               matrix[4 * r0 + c1] *
               (matrix[4 * r1 + c0] * matrix[4 * r2 + c2] - matrix[4 * r2 + c0] * matrix[4 * r1 + c2]) +
               matrix[4 * r0 + c2] *
               (matrix[4 * r1 + c0] * matrix[4 * r2 + c1] - matrix[4 * r2 + c0] * matrix[4 * r1 + c1]);
    }

    static void ToInverse(const XrVector4f in[4], XrVector4f out[4]) {
        float *matrix = (float*)in;
        float *inv_mat = (float*)out;
        const float rcpDet =
                1.0f / (matrix[0] * ToMinor(matrix, 1, 2, 3, 1, 2, 3) -
                        matrix[1] * ToMinor(matrix, 1, 2, 3, 0, 2, 3) +
                        matrix[2] * ToMinor(matrix, 1, 2, 3, 0, 1, 3) -
                        matrix[3] * ToMinor(matrix, 1, 2, 3, 0, 1, 2));

        inv_mat[0] = ToMinor(matrix, 1, 2, 3, 1, 2, 3) * rcpDet;
        inv_mat[1] = -ToMinor(matrix, 0, 2, 3, 1, 2, 3) * rcpDet;
        inv_mat[2] = ToMinor(matrix, 0, 1, 3, 1, 2, 3) * rcpDet;
        inv_mat[3] = -ToMinor(matrix, 0, 1, 2, 1, 2, 3) * rcpDet;
        inv_mat[4] = -ToMinor(matrix, 1, 2, 3, 0, 2, 3) * rcpDet;
        inv_mat[5] = ToMinor(matrix, 0, 2, 3, 0, 2, 3) * rcpDet;
        inv_mat[6] = -ToMinor(matrix, 0, 1, 3, 0, 2, 3) * rcpDet;
        inv_mat[7] = ToMinor(matrix, 0, 1, 2, 0, 2, 3) * rcpDet;
        inv_mat[8] = ToMinor(matrix, 1, 2, 3, 0, 1, 3) * rcpDet;
        inv_mat[9] = -ToMinor(matrix, 0, 2, 3, 0, 1, 3) * rcpDet;
        inv_mat[10] = ToMinor(matrix, 0, 1, 3, 0, 1, 3) * rcpDet;
        inv_mat[11] = -ToMinor(matrix, 0, 1, 2, 0, 1, 3) * rcpDet;
        inv_mat[12] = -ToMinor(matrix, 1, 2, 3, 0, 1, 2) * rcpDet;
        inv_mat[13] = ToMinor(matrix, 0, 2, 3, 0, 1, 2) * rcpDet;
        inv_mat[14] = -ToMinor(matrix, 0, 1, 3, 0, 1, 2) * rcpDet;
        inv_mat[15] = ToMinor(matrix, 0, 1, 2, 0, 1, 2) * rcpDet;
    }

    static void Projection(XrVector4f result[4], const float fov_x, const float fov_y,
                                 const float nearZ, const float farZ) {
        float *projectionMatrix = (float*)result;

        float	xmin, xmax, ymin, ymax;
        float	width, height, depth;


        ymax = nearZ * tan( fov_y );
        ymin = -ymax;

        xmax = nearZ * tan( fov_x );
        xmin = -xmax;

        width = xmax - xmin;
        height = ymax - ymin;
        depth = farZ - nearZ;

        projectionMatrix[0] = 2 * nearZ / width;
        projectionMatrix[4] = 0;
        projectionMatrix[8] = ( xmax + xmin ) / width;
        projectionMatrix[12] = 0;

        projectionMatrix[1] = 0;
        projectionMatrix[5] = 2 * nearZ / height;
        projectionMatrix[9] = ( ymax + ymin ) / height;
        projectionMatrix[13] = 0;

        projectionMatrix[2] = 0;
        projectionMatrix[6] = 0;
        projectionMatrix[10] = -( farZ + nearZ ) / depth;
        projectionMatrix[14] = -2 * farZ * nearZ / depth;

        projectionMatrix[3] = 0;
        projectionMatrix[7] = 0;
        projectionMatrix[11] = -1;
        projectionMatrix[15] = 0;
    }
};

class Quatf {
public:
    static XrQuaternionf Identity() { return XrQuaternionf{0.0f, 0.0f, 0.0f, 1.0f}; }

    // Given a yaw (Y-axis), pitch (X-axis) and roll (Z-axis) in radians, create
    // a quaternion representing the same rotation
    static XrQuaternionf
    FromEuler(const float yawInRadians, const float pitchInRadians, const float rollInRadians) {
        // Calculate half angles
        const float halfPitch = pitchInRadians * 0.5f;
        const float halfYaw   = yawInRadians * 0.5f;
        const float halfRoll  = rollInRadians * 0.5f;

        // Calculate sin and cos for each half angle
        const float sinPitch = std::sin(halfPitch);
        const float cosPitch = std::cos(halfPitch);
        const float sinYaw   = std::sin(halfYaw);
        const float cosYaw   = std::cos(halfYaw);
        const float sinRoll  = std::sin(halfRoll);
        const float cosRoll  = std::cos(halfRoll);

        // Create quaternions for each rotation
        const XrQuaternionf qx = {sinPitch, 0.0f, 0.0f, cosPitch};
        const XrQuaternionf qy = {0.0f, sinYaw, 0.0f, cosYaw};
        const XrQuaternionf qz = {0.0f, 0.0f, sinRoll, cosRoll};

        // Rotation order: roll * pitch * yaw
        return qz * qx * qy;
    }

    // Yaw (around Y-axis)
    static float GetYawInRadians(const XrQuaternionf& q) {
        assert(IsNormalized(q));
        return atan2f(2.0f * (q.x * q.y + q.w * q.z),
                      q.w * q.w + q.x * q.x - q.y * q.y - q.z * q.z);
    }

    // Pitch (around X-axis)
    static float GetPitchInRadians(const XrQuaternionf& q) {
        assert(IsNormalized(q));
        return asinf(-2.0f * (q.x * q.z - q.w * q.y));
    }

    // Roll (around Z-axis)
    static float GetRollInRadians(const XrQuaternionf& q) {
        assert(IsNormalized(q));
        return atan2f(2.0f * (q.y * q.z + q.w * q.x),
                      q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z);
    }

    [[maybe_unused]] static bool IsNormalized(const XrQuaternionf& q) {
        return fabs(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w - 1.0f) < kEpsilon;
    }

    static XrQuaternionf Inverted(const XrQuaternionf& q) {
        assert(IsNormalized(q));
        return XrQuaternionf{-q.x, -q.y, -q.z, q.w};
    }
    // Formula: v' = q * v * q*
    // Where q* is the conjugate of q
    static XrVector3f Rotate(const XrQuaternionf& q, const XrVector3f& _v) {
        assert(IsNormalized(q));

        const float vx = 2 * (q.y * _v.z - q.z * _v.y);
        const float vy = 2 * (q.z * _v.x - q.x * _v.z);
        const float vz = 2 * (q.x * _v.y - q.y * _v.x);

        return XrVector3f{_v.x + q.w * vx + (q.y * vz - q.z * vy),
                          _v.y + q.w * vy + (q.z * vx - q.x * vz),
                          _v.z + q.w * vz + (q.x * vy - q.y * vx)};
    }

    // Compute a quaternion representing a rotation between three orthogonal
    // basis vectors. These vectors correspond to the forward, up, and right
    // directions of a rotation matrix.
    static XrQuaternionf
    FromThreeVectors(const XrVector3f& forward, const XrVector3f& up, const XrVector3f& right) {
        const float trace = right.x + up.y + forward.z;
        if (trace > 0.0f) {
            const float s = 0.5f / sqrtf(trace + 1.0f);
            return XrQuaternionf{(up.z - forward.y) * s, (forward.x - right.z) * s,
                                 (right.y - up.x) * s, 0.25f / s};
        }
        if (right.x > up.y && right.x > forward.z) {
            const float s = 2.0f * sqrtf(1.0f + right.x - up.y - forward.z);
            return XrQuaternionf{0.25f * s, (up.x + right.y) / s, (forward.x + right.z) / s,
                                 (up.z - forward.y) / s};
        } else if (up.y > forward.z) {
            const float s = 2.0f * sqrtf(1.0f + up.y - right.x - forward.z);
            return XrQuaternionf{(up.x + right.y) / s, 0.25f * s, (forward.y + up.z) / s,
                                 (forward.x - right.z) / s};
        } else {
            const float s = 2.0f * sqrtf(1.0f + forward.z - right.x - up.y);
            return XrQuaternionf{(forward.x + right.z) / s, (forward.y + up.z) / s, 0.25f * s,
                                 (up.z - forward.y) / s};
        }
    }

    static void ToRotationMatrix(const XrQuaternionf& q, float rotation[16]) {

        float x2  = q.x + q.x;
        float y2  = q.y + q.y;
        float z2  = q.z + q.z;
        float xx2 = q.x * x2;
        float xy2 = q.x * y2;
        float xz2 = q.x * z2;
        float yy2 = q.y * y2;
        float yz2 = q.y * z2;
        float zz2 = q.z * z2;
        float sx2 = q.w * x2;
        float sy2 = q.w * y2;
        float sz2 = q.w * z2;

        float r[16] = {1 - (yy2 + zz2),  xy2 + sz2,        xz2 - sy2,        0.f, // column 0
                       xy2 - sz2,        1 - (xx2 + zz2),  yz2 + sx2,        0.f, // column 1
                       xz2 + sy2,        yz2 - sx2,        1 - (xx2 + yy2),  0.f, // column 2
                       0.f,                0.f,                0.f,                1};// column 3

        std::memcpy(rotation, r, sizeof(float ) * 16);
    }

    static void ToVectors(const XrQuaternionf& q, XrVector3f& forward,
                          XrVector3f& right, XrVector3f& up) {
        XrVector3f mat[3];
        const float ww = q.w * q.w;
        const float xx = q.x * q.x;
        const float yy = q.y * q.y;
        const float zz = q.z * q.z;

        mat[0] = {
                ww + xx - yy - zz,
                2 * (q.x * q.y - q.w * q.z),
                2 * (q.x * q.z + q.w * q.y)};

        mat[1] = {
                2 * (q.x * q.y + q.w * q.z),
                ww - xx + yy - zz,
                2 * (q.y * q.z - q.w * q.x)};

        mat[2] = {
                2 * (q.x * q.z - q.w * q.y),
                2 * (q.y * q.z + q.w * q.x),
                ww - xx - yy + zz};

        XrVector3f glFlip[3] = {{0, 0, -1},
                                 {1, 0, 0},
                                 {0, 1, 0}};

        XrVector3f f = Matrixf::XrVector3f_Multiply(mat, glFlip[0]);
        XrVector3f r = Matrixf::XrVector3f_Multiply(mat, glFlip[1]);
        XrVector3f u = Matrixf::XrVector3f_Multiply(mat, glFlip[2]);

        forward = {-f.z, -f.x, f.y};
        right = {-r.z, -r.x, r.y};
        up = {-u.z, -u.x, u.y};
    }
};

class Posef {
public:
    static XrPosef Identity() { return XrPosef{Quatf::Identity(), {0.0f, 0.0f, 0.0f}}; }

    static XrVector3f Transform(const XrPosef& pose, const XrVector3f& vector) {
        return Quatf::Rotate(pose.orientation, vector) + pose.position;
    }

    static XrPosef Inverted(const XrPosef& pose) {
        const XrQuaternionf invOrientation = Quatf::Inverted(pose.orientation);

        XrVector3f invPosition = Quatf::Rotate(invOrientation, pose.position);
        invPosition.x          = -invPosition.x;
        invPosition.y          = -invPosition.y;
        invPosition.z          = -invPosition.z;

        return {invOrientation, invPosition};
    }
};

} // namespace XrMath
