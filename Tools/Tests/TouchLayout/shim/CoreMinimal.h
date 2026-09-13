// Shim mínimo de los tipos de Unreal, sólo para poder compilar y ejecutar
// PaxTouchLayout.cpp fuera del motor.
#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>

using int32  = std::int32_t;
using uint8  = std::uint8_t;
using uint32 = std::uint32_t;
using TCHAR  = wchar_t;
#define TEXT(x) L##x

#define USTRUCT(...)
#define UENUM(...)
#define UCLASS(...)
#define UPROPERTY(...)
#define UFUNCTION(...)
#define GENERATED_BODY()
#define UMETA(...)
#define PAX_API

enum EForceInit { ForceInit };

struct FVector2D
{
    double X = 0.0, Y = 0.0;
    FVector2D() = default;
    FVector2D(double InX, double InY) : X(InX), Y(InY) {}
    static const FVector2D ZeroVector;
    bool Equals(const FVector2D& O, double Tol) const
    { return std::fabs(X - O.X) <= Tol && std::fabs(Y - O.Y) <= Tol; }
    FVector2D operator-(const FVector2D& O) const { return FVector2D(X - O.X, Y - O.Y); }
};
inline const FVector2D FVector2D::ZeroVector{0.0, 0.0};

struct FBox2D
{
    FVector2D Min, Max;
    FBox2D() = default;
    explicit FBox2D(EForceInit) : Min(0,0), Max(0,0) {}
    FBox2D(const FVector2D& InMin, const FVector2D& InMax) : Min(InMin), Max(InMax) {}
    bool IsInside(const FVector2D& P) const
    { return P.X >= Min.X && P.X <= Max.X && P.Y >= Min.Y && P.Y <= Max.Y; }
    FVector2D GetSize() const { return FVector2D(Max.X - Min.X, Max.Y - Min.Y); }
};

struct FMath
{
    template<typename T> static T Clamp(T V, T Lo, T Hi) { return std::max(Lo, std::min(V, Hi)); }
    template<typename T> static T Max(T A, T B) { return std::max(A, B); }
    template<typename T> static T Min(T A, T B) { return std::min(A, B); }
};
