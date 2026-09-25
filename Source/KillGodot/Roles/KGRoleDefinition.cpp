#include "Roles/KGRoleDefinition.h"

EKGAlignment FKGRoleInfo::GetAlignment() const
{
	switch (Faction)
	{
	case EKGFaction::Town:
		return EKGAlignment::Town;
	case EKGFaction::Neutral:
		return EKGAlignment::Neutral;
	default:
		return EKGAlignment::Impatient;
	}
}
