// Copyright Pax. All Rights Reserved.

#include "F1Wheel.h"

UF1Wheel::UF1Wheel()
{
	// Neumático de 18": 720 mm de diámetro exterior.
	WheelRadius = 36.f;
	WheelMass = 13.f;

	// Slick sin dibujo sobre asfalto seco: coeficiente muy por encima de un
	// neumático de calle. El modelo de desgaste (UTyreComponent) escala este
	// valor en tiempo real según temperatura y vida del compuesto.
	FrictionForceMultiplier = 3.0f;
	CorneringStiffness = 2500.f;
	SideSlipModifier = 1.0f;
	SlipThreshold = 20.f;
	SkidThreshold = 20.f;

	// Suspensión de monoplaza: recorrido corto y muy amortiguada.
	SuspensionMaxRaise = 4.f;
	SuspensionMaxDrop = 5.f;
	SuspensionDampingRatio = 0.85f;
	SpringRate = 900.f;
	SpringPreload = 120.f;
	SuspensionSmoothing = 4;
	WheelLoadRatio = 0.6f;
	RollbarScaling = 0.6f;
}

UF1WheelFront::UF1WheelFront()
{
	AxleType = EAxleType::Front;
	WheelWidth = 30.5f;

	bAffectedBySteering = true;
	bAffectedByEngine = false;
	bAffectedByBrake = true;
	bAffectedByHandbrake = false;

	// ~18° en rueda es lo que da la cremallera de un F1 actual.
	MaxSteerAngle = 18.f;
	MaxBrakeTorque = 6000.f;
	MaxHandBrakeTorque = 0.f;
}

UF1WheelRear::UF1WheelRear()
{
	AxleType = EAxleType::Rear;
	WheelWidth = 40.5f;

	bAffectedBySteering = false;
	bAffectedByEngine = true;
	bAffectedByBrake = true;
	bAffectedByHandbrake = true;

	MaxSteerAngle = 0.f;
	MaxBrakeTorque = 3500.f;
	// El freno de mano no existe en un F1; se mantiene con par bajo porque
	// Chaos lo usa para inmovilizar el coche en parrilla y en el pit box.
	MaxHandBrakeTorque = 4000.f;
}
