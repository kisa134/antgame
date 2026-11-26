#include "AntState.h"
#include "AntKitchenGameMode.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"

void AntState::Set(UPrimitiveComponent* C, EKind Kind, float Value)
{
	if (!C || Kind >= Count) { return; }
	const float V = FMath::Clamp(Value, 0.f, 1.f);
	if (FMath::IsNearlyEqual(Get(C, Kind), V, 0.004f)) { return; }
	C->SetCustomPrimitiveDataFloat((int32)Kind, V);
}

float AntState::Get(const UPrimitiveComponent* C, EKind Kind)
{
	if (!C) { return 0.f; }
	const TArray<float>& D = C->GetCustomPrimitiveData().Data;
	return D.IsValidIndex((int32)Kind) ? D[(int32)Kind] : 0.f;
}

int32 AntState::SetAround(UWorld* World, const FVector& At, float Radius, EKind Kind, float Value)
{
	int32 N = 0;
	if (!World) { return 0; }
	for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
	{
		UStaticMeshComponent* C = *It;
		if (!C || C->GetWorld() != World || !C->IsRegistered() || !C->GetStaticMesh()) { continue; }
		if (FVector::Dist(C->Bounds.Origin, At) > Radius) { continue; }
		Set(C, Kind, Value);
		++N;
	}
	return N;
}

bool AntState::DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A)
{
	if (!GM || A.Num() < 2 || A[0].ToLower() != TEXT("state")) { return false; }
	const FString Sub = A[1].ToLower();
	auto Num = [&A](int32 i, float Def = 0.f) { return A.IsValidIndex(i) ? FCString::Atof(*A[i]) : Def; };
	const FVector At(Num(3), Num(4), Num(5));
	const float R = Num(6, 300.f);
	int32 N = 0;
	if (Sub == TEXT("clear")) { for (uint8 k = 0; k < Count; ++k) { N = SetAround(GM->GetWorld(), At, R, (EKind)k, 0.f); } }
	else
	{
		const int32 Kind = Sub == TEXT("wet") ? Wet : Sub == TEXT("char") ? Char : Sub == TEXT("dirt") ? Dirt : Sub == TEXT("damage") ? Damage : -1;
		if (Kind < 0) { return false; }
		N = SetAround(GM->GetWorld(), At, R, (EKind)Kind, Num(2, 1.f));
	}
	UE_LOG(LogTemp, Display, TEXT("ANTSTATE %s %.2f at %s r %.0f: %d components"), *Sub, Num(2, 1.f), *At.ToCompactString(), R, N);
	return true;
}