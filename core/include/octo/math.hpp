#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace octo::math {
template <std::size_t N> struct Jet2 {
    double v{};
    std::array<double, N> d{};
    std::array<double, N * N> h{};
    Jet2() = default;
    Jet2(double a) : v(a) {}
    static Jet2 variable(double a, std::size_t j) {
        Jet2 b(a);
        b.d[j] = 1;
        return b;
    }
    Jet2 operator-() const {
        Jet2 b(-v);
        for (std::size_t j = 0; j < N; ++j)
            b.d[j] = -d[j];
        for (std::size_t j = 0; j < N * N; ++j)
            b.h[j] = -h[j];
        return b;
    }
    friend Jet2 operator+(Jet2 a, const Jet2 &b) {
        a.v += b.v;
        for (std::size_t j = 0; j < N; ++j)
            a.d[j] += b.d[j];
        for (std::size_t j = 0; j < N * N; ++j)
            a.h[j] += b.h[j];
        return a;
    }
    friend Jet2 operator-(Jet2 a, const Jet2 &b) { return a + -b; }
    friend Jet2 operator*(const Jet2 &a, const Jet2 &b) {
        Jet2 c(a.v * b.v);
        for (std::size_t i = 0; i < N; ++i) {
            c.d[i] = a.d[i] * b.v + a.v * b.d[i];
            for (std::size_t j = 0; j < N; ++j)
                c.h[i * N + j] =
                    a.h[i * N + j] * b.v + a.v * b.h[i * N + j] + a.d[i] * b.d[j] + b.d[i] * a.d[j];
        }
        return c;
    }
    static Jet2 unary(const Jet2 &a, double f, double fp, double fpp) {
        Jet2 b(f);
        for (std::size_t i = 0; i < N; ++i) {
            b.d[i] = fp * a.d[i];
            for (std::size_t j = 0; j < N; ++j)
                b.h[i * N + j] = fp * a.h[i * N + j] + fpp * a.d[i] * a.d[j];
        }
        return b;
    }
    friend Jet2 operator/(const Jet2 &a, const Jet2 &b) {
        return a * unary(b, 1 / b.v, -1 / (b.v * b.v), 2 / (b.v * b.v * b.v));
    }
};
template <std::size_t N> double value(const Jet2<N> &a) { return a.v; }
template <std::size_t N> Jet2<N> sin(const Jet2<N> &a) {
    return Jet2<N>::unary(a, std::sin(a.v), std::cos(a.v), -std::sin(a.v));
}
template <std::size_t N> Jet2<N> cos(const Jet2<N> &a) {
    return Jet2<N>::unary(a, std::cos(a.v), -std::sin(a.v), -std::cos(a.v));
}
template <std::size_t N> Jet2<N> sqrt(const Jet2<N> &a) {
    double r = std::sqrt(a.v);
    return Jet2<N>::unary(a, r, 0.5 / r, -0.25 / (a.v * r));
}
template <std::size_t N> Jet2<N> atan2(const Jet2<N> &y, const Jet2<N> &x) {
    Jet2<N> b(std::atan2(y.v, x.v));
    double r = x.v * x.v + y.v * y.v, r2 = r * r, fx = -y.v / r, fy = x.v / r,
           fxx = 2 * x.v * y.v / r2, fyy = -fxx, fxy = (y.v * y.v - x.v * x.v) / r2;
    for (std::size_t i = 0; i < N; ++i) {
        b.d[i] = fx * x.d[i] + fy * y.d[i];
        for (std::size_t j = 0; j < N; ++j)
            b.h[i * N + j] = fx * x.h[i * N + j] + fy * y.h[i * N + j] + fxx * x.d[i] * x.d[j] +
                             fyy * y.d[i] * y.d[j] + fxy * (x.d[i] * y.d[j] + y.d[i] * x.d[j]);
    }
    return b;
}
inline constexpr double pi = 3.1415926535897932384626433832795;
// Forward automatic differentiation. Element derivatives have 2 x SE(3) = 12 DOFs.
template <std::size_t N> struct Dual {
    double v{};
    std::array<double, N> d{};
    Dual() = default;
    Dual(double value) : v(value) {}
    static Dual variable(double value, std::size_t i) {
        Dual a(value);
        a.d[i] = 1;
        return a;
    }
    Dual operator-() const {
        Dual a(-v);
        for (std::size_t i = 0; i < N; ++i)
            a.d[i] = -d[i];
        return a;
    }
    friend Dual operator+(Dual a, const Dual &b) {
        a.v += b.v;
        for (std::size_t i = 0; i < N; ++i)
            a.d[i] += b.d[i];
        return a;
    }
    friend Dual operator-(Dual a, const Dual &b) { return a + -b; }
    friend Dual operator*(const Dual &a, const Dual &b) {
        Dual c(a.v * b.v);
        for (std::size_t i = 0; i < N; ++i)
            c.d[i] = a.d[i] * b.v + a.v * b.d[i];
        return c;
    }
    friend Dual operator/(const Dual &a, const Dual &b) {
        Dual c(a.v / b.v);
        for (std::size_t i = 0; i < N; ++i)
            c.d[i] = (a.d[i] - c.v * b.d[i]) / b.v;
        return c;
    }
};
inline double value(double a) { return a; }
template <std::size_t N> double value(const Dual<N> &a) { return a.v; }
using std::atan2;
using std::cos;
using std::sin;
using std::sqrt;
template <std::size_t N> Dual<N> sqrt(const Dual<N> &a) {
    Dual<N> b(std::sqrt(a.v));
    for (std::size_t i = 0; i < N; ++i)
        b.d[i] = a.d[i] / (2 * b.v);
    return b;
}
template <std::size_t N> Dual<N> sin(const Dual<N> &a) {
    Dual<N> b(std::sin(a.v));
    for (std::size_t i = 0; i < N; ++i)
        b.d[i] = a.d[i] * std::cos(a.v);
    return b;
}
template <std::size_t N> Dual<N> cos(const Dual<N> &a) {
    Dual<N> b(std::cos(a.v));
    for (std::size_t i = 0; i < N; ++i)
        b.d[i] = -a.d[i] * std::sin(a.v);
    return b;
}
template <std::size_t N> Dual<N> atan2(const Dual<N> &y, const Dual<N> &x) {
    Dual<N> b(std::atan2(y.v, x.v));
    double z = x.v * x.v + y.v * y.v;
    for (std::size_t i = 0; i < N; ++i)
        b.d[i] = (x.v * y.d[i] - y.v * x.d[i]) / z;
    return b;
}
template <class T> struct Vec {
    T x{}, y{}, z{};
    Vec() = default;
    Vec(T a, T b, T c) : x(a), y(b), z(c) {}
    template <class U> explicit Vec(const Vec<U> &a) : x(a.x), y(a.y), z(a.z) {}
    T &operator[](std::size_t i) { return i == 0 ? x : (i == 1 ? y : z); }
    const T &operator[](std::size_t i) const { return i == 0 ? x : (i == 1 ? y : z); }
    friend Vec operator+(Vec a, Vec b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
    friend Vec operator-(Vec a, Vec b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
    Vec operator-() const { return {-x, -y, -z}; }
    friend Vec operator*(Vec a, T b) { return {a.x * b, a.y * b, a.z * b}; }
    friend Vec operator*(T b, Vec a) { return a * b; }
    friend Vec operator/(Vec a, T b) { return {a.x / b, a.y / b, a.z / b}; }
    Vec &operator+=(Vec b) {
        *this = *this + b;
        return *this;
    }
};
template <class T> T dot(Vec<T> a, Vec<T> b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
template <class T> Vec<T> cross(Vec<T> a, Vec<T> b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
template <class T> T norm(Vec<T> a) { return sqrt(dot(a, a)); }
using V3 = Vec<double>;
inline bool finite(V3 a) { return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z); }
template <class T> struct Quat {
    T w{1}, x{}, y{}, z{};
    Quat() = default;
    Quat(T a, T b, T c, T d) : w(a), x(b), y(c), z(d) {}
    template <class U> explicit Quat(const Quat<U> &a) : w(a.w), x(a.x), y(a.y), z(a.z) {}
    Quat conjugate() const { return {w, -x, -y, -z}; }
    Quat operator-() const { return {-w, -x, -y, -z}; }
    friend Quat operator*(Quat a, Quat b) {
        return {a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
                a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w};
    }
    Vec<T> rotate(Vec<T> a) const {
        auto v = Vec<T>{x, y, z};
        return a + cross(v, a) * (T(2) * w) + cross(v, cross(v, a)) * T(2);
    }
};
using Q = Quat<double>;
template <class T> Quat<T> normalized(Quat<T> q) {
    T n = sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    return {q.w / n, q.x / n, q.y / n, q.z / n};
}
template <class T> Quat<T> exp(Vec<T> r) {
    T t2 = dot(r, r), s, c;
    if (value(t2) < 1e-10) {
        s = T(0.5) - t2 / T(48) + t2 * t2 / T(3840);
        c = T(1) - t2 / T(8) + t2 * t2 / T(384);
    } else {
        T t = sqrt(t2);
        s = sin(t / T(2)) / t;
        c = cos(t / T(2));
    }
    return {c, s * r.x, s * r.y, s * r.z};
}
template <class T> Vec<T> log(Quat<T> q) {
    q = normalized(q);
    if (value(q.w) < 0)
        q = -q;
    T s2 = q.x * q.x + q.y * q.y + q.z * q.z;
    T f;
    if (value(s2) < 1e-12)
        f = T(2) + s2 / T(3) + T(3) * s2 * s2 / T(20);
    else {
        T s = sqrt(s2);
        f = T(2) * atan2(s, q.w) / s;
    }
    return {f * q.x, f * q.y, f * q.z};
}
template <class T> Quat<T> midpoint(Quat<T> a, Quat<T> b) {
    if (value(a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z) < 0)
        b = -b;
    return normalized(Quat<T>{a.w + b.w, a.x + b.x, a.y + b.y, a.z + b.z});
}
} // namespace octo::math
