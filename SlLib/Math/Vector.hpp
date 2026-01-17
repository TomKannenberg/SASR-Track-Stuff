#pragma once

#include <array>
#include <cstddef>
#include <cmath>
#include <limits>

namespace SlLib::Math {

struct Vector2
{
    float X{};
    float Y{};
};

struct Vector3
{
    float X{};
    float Y{};
    float Z{};
};

struct Vector4
{
    float X{};
    float Y{};
    float Z{};
    float W{};
};

struct Matrix4x4
{
    std::array<std::array<float, 4>, 4> M{};

    float& operator()(std::size_t row, std::size_t column)
    {
        return M[row][column];
    }

    float operator()(std::size_t row, std::size_t column) const
    {
        return M[row][column];
    }
};

struct Quaternion
{
    float X{};
    float Y{};
    float Z{};
    float W{1.0f};
};

inline Vector3 operator*(Vector3 const& value, float scalar)
{
    return {value.X * scalar, value.Y * scalar, value.Z * scalar};
}

inline Vector3 operator*(float scalar, Vector3 const& value)
{
    return value * scalar;
}

inline Vector3 operator-(Vector3 const& value)
{
    return {-value.X, -value.Y, -value.Z};
}

inline float dot(Vector3 const& a, Vector3 const& b)
{
    return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
}

inline Vector3 cross(Vector3 const& a, Vector3 const& b)
{
    return {a.Y * b.Z - a.Z * b.Y,
            a.Z * b.X - a.X * b.Z,
            a.X * b.Y - a.Y * b.X};
}

inline float length(Vector3 const& value)
{
    return std::sqrt(dot(value, value));
}

inline Vector3 normalize(Vector3 value)
{
    float len = length(value);
    if (len == 0.0f) {
        return value;
    }
    return value * (1.0f / len);
}

inline Quaternion operator*(Quaternion const& lhs, Quaternion const& rhs)
{
    return {
        lhs.W * rhs.X + lhs.X * rhs.W + lhs.Y * rhs.Z - lhs.Z * rhs.Y,
        lhs.W * rhs.Y - lhs.X * rhs.Z + lhs.Y * rhs.W + lhs.Z * rhs.X,
        lhs.W * rhs.Z + lhs.X * rhs.Y - lhs.Y * rhs.X + lhs.Z * rhs.W,
        lhs.W * rhs.W - lhs.X * rhs.X - lhs.Y * rhs.Y - lhs.Z * rhs.Z};
}

inline Quaternion operator*(Quaternion const& value, float scalar)
{
    return {value.X * scalar, value.Y * scalar, value.Z * scalar, value.W * scalar};
}

inline Quaternion CreateFromAxisAngle(Vector3 axis, float angle)
{
    axis = normalize(axis);
    float halfAngle = angle * 0.5f;
    float sinHalf = std::sin(halfAngle);
    float cosHalf = std::cos(halfAngle);
    return {axis.X * sinHalf, axis.Y * sinHalf, axis.Z * sinHalf, cosHalf};
}

inline Matrix4x4 CreateFromQuaternion(Quaternion const& q)
{
    Matrix4x4 result{};
    float xx = q.X * q.X;
    float yy = q.Y * q.Y;
    float zz = q.Z * q.Z;
    float xy = q.X * q.Y;
    float xz = q.X * q.Z;
    float yz = q.Y * q.Z;
    float xw = q.X * q.W;
    float yw = q.Y * q.W;
    float zw = q.Z * q.W;

    result(0, 0) = 1.0f - 2.0f * (yy + zz);
    result(0, 1) = 2.0f * (xy - zw);
    result(0, 2) = 2.0f * (xz + yw);
    result(0, 3) = 0.0f;

    result(1, 0) = 2.0f * (xy + zw);
    result(1, 1) = 1.0f - 2.0f * (xx + zz);
    result(1, 2) = 2.0f * (yz - xw);
    result(1, 3) = 0.0f;

    result(2, 0) = 2.0f * (xz - yw);
    result(2, 1) = 2.0f * (yz + xw);
    result(2, 2) = 1.0f - 2.0f * (xx + yy);
    result(2, 3) = 0.0f;

    result(3, 0) = 0.0f;
    result(3, 1) = 0.0f;
    result(3, 2) = 0.0f;
    result(3, 3) = 1.0f;

    return result;
}

inline Vector3 operator+(Vector3 const& a, Vector3 const& b)
{
    return {a.X + b.X, a.Y + b.Y, a.Z + b.Z};
}

inline Vector3 operator-(Vector3 const& a, Vector3 const& b)
{
    return {a.X - b.X, a.Y - b.Y, a.Z - b.Z};
}

inline Vector3 operator/(Vector3 const& value, float scalar)
{
    if (scalar == 0.0f) return value;
    float inv = 1.0f / scalar;
    return {value.X * inv, value.Y * inv, value.Z * inv};
}

inline Vector4 Transform(Matrix4x4 const& m, Vector4 const& v)
{
    return {
        m(0, 0) * v.X + m(0, 1) * v.Y + m(0, 2) * v.Z + m(0, 3) * v.W,
        m(1, 0) * v.X + m(1, 1) * v.Y + m(1, 2) * v.Z + m(1, 3) * v.W,
        m(2, 0) * v.X + m(2, 1) * v.Y + m(2, 2) * v.Z + m(2, 3) * v.W,
        m(3, 0) * v.X + m(3, 1) * v.Y + m(3, 2) * v.Z + m(3, 3) * v.W};
}

inline Matrix4x4 Multiply(Matrix4x4 const& a, Matrix4x4 const& b)
{
    Matrix4x4 result{};
    for (std::size_t row = 0; row < 4; ++row)
        for (std::size_t col = 0; col < 4; ++col)
            for (std::size_t i = 0; i < 4; ++i)
                result(row, col) += a(row, i) * b(i, col);
    return result;
}

inline Matrix4x4 Invert(Matrix4x4 const& m)
{
    float inv[16];
    const float* a = &m.M[0][0];
    inv[0] = a[5] * a[10] * a[15] -
             a[5] * a[11] * a[14] -
             a[9] * a[6] * a[15] +
             a[9] * a[7] * a[14] +
             a[13] * a[6] * a[11] -
             a[13] * a[7] * a[10];

    inv[4] = -a[4] * a[10] * a[15] +
              a[4] * a[11] * a[14] +
              a[8] * a[6] * a[15] -
              a[8] * a[7] * a[14] -
              a[12] * a[6] * a[11] +
              a[12] * a[7] * a[10];

    inv[8] = a[4] * a[9] * a[15] -
             a[4] * a[11] * a[13] -
             a[8] * a[5] * a[15] +
             a[8] * a[7] * a[13] +
             a[12] * a[5] * a[11] -
             a[12] * a[7] * a[9];

    inv[12] = -a[4] * a[9] * a[14] +
               a[4] * a[10] * a[13] +
               a[8] * a[5] * a[14] -
               a[8] * a[6] * a[13] -
               a[12] * a[5] * a[10] +
               a[12] * a[6] * a[9];

    inv[1] = -a[1] * a[10] * a[15] +
              a[1] * a[11] * a[14] +
              a[9] * a[2] * a[15] -
              a[9] * a[3] * a[14] -
              a[13] * a[2] * a[11] +
              a[13] * a[3] * a[10];

    inv[5] = a[0] * a[10] * a[15] -
             a[0] * a[11] * a[14] -
             a[8] * a[2] * a[15] +
             a[8] * a[3] * a[14] +
             a[12] * a[2] * a[11] -
             a[12] * a[3] * a[10];

    inv[9] = -a[0] * a[9] * a[15] +
              a[0] * a[11] * a[13] +
              a[8] * a[1] * a[15] -
              a[8] * a[3] * a[13] -
              a[12] * a[1] * a[11] +
              a[12] * a[3] * a[9];

    inv[13] = a[0] * a[9] * a[14] -
              a[0] * a[10] * a[13] -
              a[8] * a[1] * a[14] +
              a[8] * a[2] * a[13] +
              a[12] * a[1] * a[10] -
              a[12] * a[2] * a[9];

    inv[2] = a[1] * a[6] * a[15] -
             a[1] * a[7] * a[14] -
             a[5] * a[2] * a[15] +
             a[5] * a[3] * a[14] +
             a[13] * a[2] * a[7] -
             a[13] * a[3] * a[6];

    inv[6] = -a[0] * a[6] * a[15] +
              a[0] * a[7] * a[14] +
              a[4] * a[2] * a[15] -
              a[4] * a[3] * a[14] -
              a[12] * a[2] * a[7] +
              a[12] * a[3] * a[6];

    inv[10] = a[0] * a[5] * a[15] -
              a[0] * a[7] * a[13] -
              a[4] * a[1] * a[15] +
              a[4] * a[3] * a[13] +
              a[12] * a[1] * a[7] -
              a[12] * a[3] * a[5];

    inv[14] = -a[0] * a[5] * a[14] +
               a[0] * a[6] * a[13] +
               a[4] * a[1] * a[14] -
               a[4] * a[2] * a[13] -
               a[12] * a[1] * a[6] +
               a[12] * a[2] * a[5];

    inv[3] = -a[1] * a[6] * a[11] +
              a[1] * a[7] * a[10] +
              a[5] * a[2] * a[11] -
              a[5] * a[3] * a[10] -
              a[9] * a[2] * a[7] +
              a[9] * a[3] * a[6];

    inv[7] = a[0] * a[6] * a[11] -
             a[0] * a[7] * a[10] -
             a[4] * a[2] * a[11] +
             a[4] * a[3] * a[10] +
             a[8] * a[2] * a[7] -
             a[8] * a[3] * a[6];

    inv[11] = -a[0] * a[5] * a[11] +
               a[0] * a[7] * a[9] +
               a[4] * a[1] * a[11] -
               a[4] * a[3] * a[9] -
               a[8] * a[1] * a[7] +
               a[8] * a[3] * a[5];

    inv[15] = a[0] * a[5] * a[10] -
              a[0] * a[6] * a[9] -
              a[4] * a[1] * a[10] +
              a[4] * a[2] * a[9] +
              a[8] * a[1] * a[6] -
              a[8] * a[2] * a[5];

    float det = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];

    if (std::abs(det) < std::numeric_limits<float>::epsilon())
        return Matrix4x4{};

    Matrix4x4 result{};
    float invDet = 1.0f / det;
    for (int i = 0; i < 16; ++i)
        result(i / 4, i % 4) = inv[i] * invDet;

    return result;
}

} // namespace SlLib::Math
