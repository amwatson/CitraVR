/*******************************************************************************

Filename    :   XrMath.h
Content     :   Lightweight 3D math library for OpenXR.
Authors     :   Amanda M. Watson
License     :   Licensed under GPLv2 or any later version.
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

static inline XrVector2f operator*(const XrVector2f& lhs, const float rhs)
{
    return XrVector2f{lhs.x * rhs, lhs.y * rhs};
}

static inline XrVector3f operator+(const XrVector3f& lhs, const XrVector3f& rhs)
{
    return XrVector3f{lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

static inline XrVector3f operator-(const XrVector3f& lhs, const XrVector3f& rhs)
{
    return XrVector3f{lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

static inline XrVector3f operator*(const XrVector3f& lhs, const float rhs)
{
    return XrVector3f{lhs.x * rhs, lhs.y * rhs, lhs.z * rhs};
}

static inline XrVector3f operator*(const float lhs, const XrVector3f& rhs)
{
    return XrVector3f{lhs * rhs.x, lhs * rhs.y, lhs * rhs.z};
}

static inline XrQuaternionf operator*(const XrQuaternionf& lhs,
                                      const XrQuaternionf& rhs)
{
    XrQuaternionf result;
    result.x = lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y;
    result.y = lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x;
    result.z = lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w;
    result.w = lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z;
    return result;
}

namespace XrMath
{

static constexpr float kEpsilon = 0.00001f;

static inline float SafeRcpSqrt(const float x)
{
    return (x >= std::numeric_limits<float>::min())
               ? 1.0f / sqrtf(x)
               : std::numeric_limits<float>::max();
}

class Vector3f
{
public:
    static float LengthSq(const XrVector3f& v)
    {
        return v.x * v.x + v.y * v.y + v.z * v.z;
    }
    static float Length(const XrVector3f& v) { return sqrtf(LengthSq(v)); }

    static void Normalize(XrVector3f& v)
    {
        const float lengthRcp = SafeRcpSqrt(LengthSq(v));
        v.x *= lengthRcp;
        v.y *= lengthRcp;
        v.z *= lengthRcp;
    }

    static XrVector3f Normalized(const XrVector3f& v)
    {
        XrVector3f res = v;
        Normalize(res);
        return res;
    }

    static XrVector3f Cross(const XrVector3f& a, const XrVector3f& b)
    {
        return XrVector3f{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                          a.x * b.y - a.y * b.x};
    }
};

class Quatf
{
public:
    static XrQuaternionf Identity()
    {
        return XrQuaternionf{0.0f, 0.0f, 0.0f, 1.0f};
    }

    // Given a yaw (Y-axis), pitch (X-axis) and roll (Z-axis) in radians, create
    // a quaternion representing the same rotation
    static XrQuaternionf FromEuler(const float yawInRadians,
                                   const float pitchInRadians,
                                   const float rollInRadians)
    {
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
    static float GetYawInRadians(const XrQuaternionf& q)
    {
        assert(IsNormalized(q));
        return atan2f(2.0f * (q.x * q.y + q.w * q.z),
                      q.w * q.w + q.x * q.x - q.y * q.y - q.z * q.z);
    }

    // Pitch (around X-axis)
    static float GetPitchInRadians(const XrQuaternionf& q)
    {
        assert(IsNormalized(q));
        return asinf(-2.0f * (q.x * q.z - q.w * q.y));
    }

    // Roll (around Z-axis)
    static float GetRollInRadians(const XrQuaternionf& q)
    {
        assert(IsNormalized(q));
        return atan2f(2.0f * (q.y * q.z + q.w * q.x),
                      q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z);
    }

    [[maybe_unused]] static bool IsNormalized(const XrQuaternionf& q)
    {
        return fabs(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w - 1.0f) <
               kEpsilon;
    }

    static XrQuaternionf Inverted(const XrQuaternionf& q)
    {
        assert(IsNormalized(q));
        return XrQuaternionf{-q.x, -q.y, -q.z, q.w};
    }
    // Formula: v' = q * v * q*
    // Where q* is the conjugate of q
    static XrVector3f Rotate(const XrQuaternionf& q, const XrVector3f& _v)
    {
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
    static XrQuaternionf FromThreeVectors(const XrVector3f& forward,
                                          const XrVector3f& up,
                                          const XrVector3f& right)
    {
        const float trace = right.x + up.y + forward.z;
        if (trace > 0.0f)
        {
            const float s = 0.5f / sqrtf(trace + 1.0f);
            return XrQuaternionf{(up.z - forward.y) * s,
                                 (forward.x - right.z) * s,
                                 (right.y - up.x) * s, 0.25f / s};
        }
        if (right.x > up.y && right.x > forward.z)
        {
            const float s = 2.0f * sqrtf(1.0f + right.x - up.y - forward.z);
            return XrQuaternionf{0.25f * s, (up.x + right.y) / s,
                                 (forward.x + right.z) / s,
                                 (up.z - forward.y) / s};
        }
        else if (up.y > forward.z)
        {
            const float s = 2.0f * sqrtf(1.0f + up.y - right.x - forward.z);
            return XrQuaternionf{(up.x + right.y) / s, 0.25f * s,
                                 (forward.y + up.z) / s,
                                 (forward.x - right.z) / s};
        }
        else
        {
            const float s = 2.0f * sqrtf(1.0f + forward.z - right.x - up.y);
            return XrQuaternionf{(forward.x + right.z) / s,
                                 (forward.y + up.z) / s, 0.25f * s,
                                 (up.z - forward.y) / s};
        }
    }
};

class Posef
{
public:
    static XrPosef Identity()
    {
        return XrPosef{Quatf::Identity(), {0.0f, 0.0f, 0.0f}};
    }

    static XrVector3f Transform(const XrPosef& pose, const XrVector3f& vector)
    {
        return Quatf::Rotate(pose.orientation, vector) + pose.position;
    }

    static XrPosef Inverted(const XrPosef& pose)
    {
        const XrQuaternionf invOrientation = Quatf::Inverted(pose.orientation);

        XrVector3f invPosition = Quatf::Rotate(invOrientation, pose.position);
        invPosition.x          = -invPosition.x;
        invPosition.y          = -invPosition.y;
        invPosition.z          = -invPosition.z;

        return {invOrientation, invPosition};
    }
};

} // namespace XrMath
