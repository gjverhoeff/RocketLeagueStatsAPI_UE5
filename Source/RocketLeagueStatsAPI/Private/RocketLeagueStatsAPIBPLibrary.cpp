// Copyright (c) 2026 Sumatras Studios. All Rights Reserved.

#include "RocketLeagueStatsAPIBPLibrary.h"
#include "RocketLeagueStatsAPI.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

URocketLeagueStatsAPIBPLibrary::URocketLeagueStatsAPIBPLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

URocketLeagueStatsAPISocket* URocketLeagueStatsAPIBPLibrary::Connect(FString Host, int32 port, bool& bSuccessful)
{
	FSocket* MySockTemp = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("RocketLeagueStatsAPI TCP Socket"), false);
	if (MySockTemp == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create socket"));
		bSuccessful = false;
		return nullptr;
	}

	// Set socket to non-blocking mode to prevent hanging
	MySockTemp->SetNonBlocking(true);
	
	// Set receive buffer size and timeout
	int32 NewSize = 0;
	MySockTemp->SetReceiveBufferSize(65536, NewSize);
	MySockTemp->SetSendBufferSize(65536, NewSize);
	
	URocketLeagueStatsAPISocket* NetSock = NewObject<URocketLeagueStatsAPISocket>();

	FAddressInfoResult LookupResult = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetAddressInfo(*Host, nullptr, EAddressInfoFlags::Default, NAME_None);
	if (LookupResult.ReturnCode != ESocketErrors::SE_NO_ERROR || LookupResult.Results.Num() < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Unable to resolve host \"%s\"!"), *Host);
		MySockTemp->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(MySockTemp);
		bSuccessful = false;
		return nullptr;
	}

	TSharedRef<FInternetAddr> SockAddr = LookupResult.Results[0].Address;
	SockAddr->SetPort(port);

	bool connected = MySockTemp->Connect(*SockAddr);
	if (!connected)
	{
		ESocketConnectionState ConnectionState = MySockTemp->GetConnectionState();
		if (ConnectionState != SCS_Connected && ConnectionState != SCS_ConnectionError)
		{
			UE_LOG(LogTemp, Warning, TEXT("Connection in progress (non-blocking mode)"));
		}
		else if (ConnectionState == SCS_ConnectionError)
		{
			UE_LOG(LogTemp, Error, TEXT("Could not connect to %s:%d"), *Host, port);
			MySockTemp->Close();
			ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(MySockTemp);
			bSuccessful = false;
			return nullptr;
		}
	}

	NetSock->SetSocket(MySockTemp);
	bSuccessful = true;
	
	UE_LOG(LogTemp, Log, TEXT("Connected to %s:%d"), *Host, port);
	
	return NetSock;
}

bool URocketLeagueStatsAPIBPLibrary::IsConnectionValid(URocketLeagueStatsAPISocket* Connection)
{
	if (!IsValid(Connection))
	{
		return false;
	}

	FSocket* MySocket = Connection->GetSocket();
	if (MySocket == nullptr)
	{
		return false;
	}

	ESocketConnectionState State = MySocket->GetConnectionState();
	return (State == SCS_Connected);
}

bool URocketLeagueStatsAPIBPLibrary::CloseConnection(URocketLeagueStatsAPISocket* Connection)
{
	if (!IsValid(Connection))
	{
		return false;
	}

	FSocket* MySocket = Connection->GetSocket();
	if (MySocket == nullptr)
	{
		return false;
	}

	// Clear any buffered data first
	Connection->ClearBuffer();

	// Drain any remaining data from socket before closing to prevent remote crashes
	uint32 PendingSize = 0;
	if (MySocket->HasPendingData(PendingSize) && PendingSize > 0)
	{
		TArray<uint8> DrainBuffer;
		DrainBuffer.SetNum(FMath::Min(PendingSize, 65536u));
		int32 BytesRead = 0;
		MySocket->Recv(DrainBuffer.GetData(), DrainBuffer.Num(), BytesRead, ESocketReceiveFlags::None);
		UE_LOG(LogTemp, Log, TEXT("Drained %d bytes before closing"), BytesRead);
	}

	// Shutdown for writing first (signal we're done sending)
	MySocket->Shutdown(ESocketShutdownMode::Write);

	// Small delay to allow remote side to process shutdown
	FPlatformProcess::Sleep(0.1f);

	// Now shutdown reading
	MySocket->Shutdown(ESocketShutdownMode::Read);

	// Close the socket
	bool bClosed = MySocket->Close();

	// Destroy the socket to free resources
	ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(MySocket);
	Connection->SetSocket(nullptr);

	UE_LOG(LogTemp, Log, TEXT("Connection closed successfully"));

	return bClosed;
}

void URocketLeagueStatsAPIBPLibrary::ForceCloseConnection(URocketLeagueStatsAPISocket* Connection)
{
	if (!IsValid(Connection))
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Force closing connection (emergency cleanup)"));
	Connection->CleanupSocket();
}

void URocketLeagueStatsAPIBPLibrary::GameData(
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
    FString& Target)
{
    static FString LastMatchGuid;

    TSharedRef<TJsonReader<TCHAR>> JsonReader =
        TJsonReaderFactory<TCHAR>::Create(Data);

    TSharedPtr<FJsonObject> JsonObject;

    if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid())
    {
        return;
    }

    FString EventName;
    if (!JsonObject->TryGetStringField(TEXT("Event"), EventName))
    {
        return;
    }

    if (EventName != TEXT("UpdateState"))
    {
        return;
    }

    FString DataString;
    if (!JsonObject->TryGetStringField(TEXT("Data"), DataString))
    {
        return;
    }

    TSharedRef<TJsonReader<TCHAR>> DataReader =
        TJsonReaderFactory<TCHAR>::Create(DataString);

    TSharedPtr<FJsonObject> DataObject;

    if (!FJsonSerializer::Deserialize(DataReader, DataObject) || !DataObject.IsValid())
    {
        return;
    }

    FString MatchGuid;
    DataObject->TryGetStringField(TEXT("MatchGuid"), MatchGuid);

    if (!MatchGuid.IsEmpty() && MatchGuid != LastMatchGuid)
    {
        LastMatchGuid = MatchGuid;

        Arena.Empty();
        Winner.Empty();
        Team1Name.Empty();
        Team2Name.Empty();
        Target.Empty();

        TimeSeconds = 0;
        Team1Score = 0;
        Team2Score = 0;
        BallSpeed = 0;

        bIsReplay = false;
        bOvertime = false;
        bHasWinner = false;
        bHasTarget = false;
    }

    const TSharedPtr<FJsonObject>* GameObject;
    if (!DataObject->TryGetObjectField(TEXT("Game"), GameObject))
    {
        return;
    }

    Arena.Empty();
    Winner.Empty();
    Team1Name.Empty();
    Team2Name.Empty();
    Target.Empty();

    TimeSeconds = 0;
    Team1Score = 0;
    Team2Score = 0;
    BallSpeed = 0;

    bIsReplay = false;
    bOvertime = false;
    bHasWinner = false;
    bHasTarget = false;

    (*GameObject)->TryGetStringField(TEXT("Arena"), Arena);
    (*GameObject)->TryGetBoolField(TEXT("bReplay"), bIsReplay);
    (*GameObject)->TryGetBoolField(TEXT("bOvertime"), bOvertime);
    (*GameObject)->TryGetBoolField(TEXT("bHasWinner"), bHasWinner);
    (*GameObject)->TryGetBoolField(TEXT("bHasTarget"), bHasTarget);

    (*GameObject)->TryGetNumberField(TEXT("TimeSeconds"), TimeSeconds);

    if (bHasWinner)
    {
        (*GameObject)->TryGetStringField(TEXT("Winner"), Winner);
    }

    const TArray<TSharedPtr<FJsonValue>>* TeamArray;
    if ((*GameObject)->TryGetArrayField(TEXT("Teams"), TeamArray))
    {
        if (TeamArray->Num() > 0)
        {
            TSharedPtr<FJsonObject> Team = (*TeamArray)[0]->AsObject();

            if (Team.IsValid())
            {
                Team->TryGetStringField(TEXT("Name"), Team1Name);
                Team->TryGetNumberField(TEXT("Score"), Team1Score);
            }
        }

        if (TeamArray->Num() > 1)
        {
            TSharedPtr<FJsonObject> Team = (*TeamArray)[1]->AsObject();

            if (Team.IsValid())
            {
                Team->TryGetStringField(TEXT("Name"), Team2Name);
                Team->TryGetNumberField(TEXT("Score"), Team2Score);
            }
        }
    }

    const TSharedPtr<FJsonObject>* BallObject;
    if ((*GameObject)->TryGetObjectField(TEXT("Ball"), BallObject))
    {
        double SpeedValue = 0.0;

        if ((*BallObject)->TryGetNumberField(TEXT("Speed"), SpeedValue))
        {
            BallSpeed = FMath::RoundToInt(SpeedValue);
        }
    }

    if (bHasTarget)
    {
        const TSharedPtr<FJsonObject>* TargetObject;

        if ((*GameObject)->TryGetObjectField(TEXT("Target"), TargetObject))
        {
            (*TargetObject)->TryGetStringField(TEXT("Name"), Target);
        }
    }
}

void URocketLeagueStatsAPIBPLibrary::PlayerDataConverter(
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
    FString& PlayerID)
{
    static FString LastMatchGuid;

    TSharedRef<TJsonReader<TCHAR>> JsonReader =
        TJsonReaderFactory<TCHAR>::Create(Data);

    TSharedPtr<FJsonObject> JsonObject;

    if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid())
    {
        return;
    }

    FString EventName;
    if (!JsonObject->TryGetStringField(TEXT("Event"), EventName))
    {
        return;
    }

    if (EventName != TEXT("UpdateState"))
    {
        return;
    }

    FString DataString;
    if (!JsonObject->TryGetStringField(TEXT("Data"), DataString))
    {
        return;
    }

    TSharedRef<TJsonReader<TCHAR>> DataReader =
        TJsonReaderFactory<TCHAR>::Create(DataString);

    TSharedPtr<FJsonObject> DataObject;

    if (!FJsonSerializer::Deserialize(DataReader, DataObject) || !DataObject.IsValid())
    {
        return;
    }

    FString MatchGuid;
    DataObject->TryGetStringField(TEXT("MatchGuid"), MatchGuid);

    if (!MatchGuid.IsEmpty() && MatchGuid != LastMatchGuid)
    {
        LastMatchGuid = MatchGuid;

        PlayerName.Empty();
        PlayerID.Empty();

        Goals = 0;
        Assists = 0;
        Demos = 0;
        Saves = 0;
        Score = 0;
        Shots = 0;
        Speed = 0;
        Touches = 0;
        Boost = 0;
        Team = 0;
    }

    const TArray<TSharedPtr<FJsonValue>>* PlayersArray;
    if (!DataObject->TryGetArrayField(TEXT("Players"), PlayersArray))
    {
        return;
    }

    if (!PlayersArray->IsValidIndex(Player))
    {
        return;
    }

    TSharedPtr<FJsonObject> PlayerObject =
        (*PlayersArray)[Player]->AsObject();

    if (!PlayerObject.IsValid())
    {
        return;
    }

    PlayerName.Empty();
    PlayerID.Empty();

    Goals = 0;
    Assists = 0;
    Demos = 0;
    Saves = 0;
    Score = 0;
    Shots = 0;
    Speed = 0;
    Touches = 0;
    Boost = 0;
    Team = 0;

    PlayerObject->TryGetStringField(TEXT("Name"), PlayerName);
    PlayerObject->TryGetStringField(TEXT("PrimaryId"), PlayerID);

    PlayerObject->TryGetNumberField(TEXT("Goals"), Goals);
    PlayerObject->TryGetNumberField(TEXT("Assists"), Assists);
    PlayerObject->TryGetNumberField(TEXT("Demos"), Demos);
    PlayerObject->TryGetNumberField(TEXT("Saves"), Saves);
    PlayerObject->TryGetNumberField(TEXT("Score"), Score);
    PlayerObject->TryGetNumberField(TEXT("Shots"), Shots);
    PlayerObject->TryGetNumberField(TEXT("Touches"), Touches);
    PlayerObject->TryGetNumberField(TEXT("Boost"), Boost);
    PlayerObject->TryGetNumberField(TEXT("TeamNum"), Team);

    double SpeedValue = 0.0;
    if (PlayerObject->TryGetNumberField(TEXT("Speed"), SpeedValue))
    {
        Speed = FMath::RoundToInt(SpeedValue);
    }
}