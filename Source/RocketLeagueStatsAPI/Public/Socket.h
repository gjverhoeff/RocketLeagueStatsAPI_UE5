// Copyright (c) 2026 Sumatras Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Runtime/Sockets/Public/Sockets.h"
#include "Runtime/Sockets/Public/SocketSubsystem.h"
#include "Tickable.h"

#include "Socket.generated.h"

class FSocketReaderThread;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRLMessageReceived, const FString&, Data);

UCLASS(BlueprintType)
class URocketLeagueStatsAPISocket : public UObject, public FTickableGameObject
{
	GENERATED_BODY()
public:
	virtual void BeginDestroy() override;

	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

	bool SetSocket(FSocket* Socket);
	FSocket* GetSocket();

	void AppendToBuffer(const FString& Data);
	FString ExtractCompleteMessage();
	void ClearBuffer();
	bool HasCompleteJSON();
	void CleanupSocket();

	void StartBackgroundReading();
	void StopBackgroundReading();
	bool IsBackgroundReading() const { return ReaderThread != nullptr; }

	void StartAutoPolling(float PollingInterval = 0.016f);
	void StopAutoPolling();
	bool IsAutoPolling() const { return bAutoPolling; }

	UPROPERTY(BlueprintAssignable, Category = "Rocket League Stats")
	FOnRLMessageReceived OnMessageReceived;

private:
	FSocket* _Socket;
	FString MessageBuffer;
	FSocketReaderThread* ReaderThread;
	bool bAutoPolling = false;
	float PollingInterval = 0.016f;
	float TimeSinceLastPoll = 0.0f;
	void PollForData();
};