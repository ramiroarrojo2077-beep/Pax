// Copyright Pax. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Unidades.
 *
 * Unreal trabaja en centímetros y kilogramos, así que una fuerza pasada a
 * AddForce está en kg·cm/s². El modelo aerodinámico y el de neumáticos se
 * escriben en Newtons (kg·m/s²) porque es lo que aparece en cualquier tabla de
 * ingeniería, y se convierten justo antes de aplicarlos.
 *
 * 1 N = 1 kg·m/s² = 100 kg·cm/s²
 */
namespace PaxUnits
{
	/** Multiplicador para pasar Newtons a unidades de fuerza de Unreal. */
	static constexpr float NewtonsToUnreal = 100.f;

	/** cm/s -> m/s */
	static constexpr float CmsToMs = 0.01f;

	/** cm/s -> km/h */
	static constexpr float CmsToKph = 0.036f;

	/** km/h -> cm/s */
	static constexpr float KphToCms = 1.f / 0.036f;

	/** Aceleración de la gravedad en m/s². */
	static constexpr float GravityMs2 = 9.81f;
}
