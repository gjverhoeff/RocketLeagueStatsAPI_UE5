// Copyright (c) 2026 Sumatras Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Socket.h"
#include "Runtime/Sockets/Public/Sockets.h"
#include "Runtime/Sockets/Public/SocketSubsystem.h"
#include "RocketLeagueStatsAPIBPLibrary.generated.h"



UCLASS()
class URocketLeagueStatsAPIBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Connect to a Rocket League Stats API", Keywords = "RocketLeagueStatsAPI connect tcp tcpconnect rocketleaguestatsapiconnect"), Category = "Networking|RocketLeagueStatsAPI")
	static URocketLeagueStatsAPISocket* Connect(FString Host, int32 port, bool& success);

	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (DisplayName = "Is Connection Valid", Keywords = "RocketLeagueStatsAPI check valid connection status"), Category = "Networking|RocketLeagueStatsAPI")
	static bool IsConnectionValid(URocketLeagueStatsAPISocket* Connection);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Close connection to TCP server", Keywords = "RocketLeagueStatsAPI disconnect close tcpclose tcp tcpdisconnect rocketleaguestatsapidisconnect"), Category = "Networking|RocketLeagueStatsAPI")
	static bool CloseConnection(URocketLeagueStatsAPISocket* Connection);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Force Close Connection (Emergency)", Keywords = "RocketLeagueStatsAPI force close emergency"), Category = "Networking|RocketLeagueStatsAPI")
	static void ForceCloseConnection(URocketLeagueStatsAPISocket* Connection);


    UFUNCTION(BlueprintCallable, Category = "Rocket League Stats API")
    static void GameData(
        const FString& Data,
        FString& Arena,
        int32& TimeSeconds,
        bool& bIsReplay,
        bool& bOvertime,
        bool& bHasWinner,
        FString& Winner,
        FString& Team1Name,
        FString& Team2Name,
        int32& Team1Score,
        int32& Team2Score,
        int32& BallSpeed,
        bool& bHasTarget,
        FString& Target);

    UFUNCTION(BlueprintCallable, Category = "Rocket League Stats API")
    static void PlayerDataConverter(
        const FString& Data,
        int32 Player,
        FString& PlayerName,
        int32& Goals,
        int32& Assists,
        int32& Demos,
        int32& Saves,
        int32& Score,
        int32& Shots,
        int32& Speed,
        int32& Touches,
        int32& Boost,
        int32& Team,
        FString& PlayerID);


};
