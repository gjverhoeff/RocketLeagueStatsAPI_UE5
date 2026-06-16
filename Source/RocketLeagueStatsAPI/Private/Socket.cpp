// Copyright (c) 2026 Sumatras Studios. All Rights Reserved.

#include "Socket.h"
#include "RocketLeagueStatsAPI.h"
#include "SocketReaderThread.h"

DEFINE_LOG_CATEGORY_STATIC(LogRLSocket, Log, All);

void URocketLeagueStatsAPISocket::BeginDestroy()
{
	StopBackgroundReading();
	StopAutoPolling();
	CleanupSocket();
	Super::BeginDestroy();
}

void URocketLeagueStatsAPISocket::CleanupSocket()
{
	if (_Socket != nullptr)
	{
		UE_LOG(LogTemp, Log, TEXT("Cleaning up socket"));
		MessageBuffer.Empty();
		_Socket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(_Socket);
		_Socket = nullptr;
	}
}

bool URocketLeagueStatsAPISocket::SetSocket(FSocket* Socket)
{
	if (_Socket != nullptr && _Socket != Socket)
	{
		UE_LOG(LogTemp, Warning, TEXT("Replacing existing socket"));
		StopBackgroundReading();
		CleanupSocket();
	}
	
	_Socket = Socket;
	MessageBuffer.Empty();
	ReaderThread = nullptr;
	
	StartBackgroundReading();
	
	return true;
}

FSocket* URocketLeagueStatsAPISocket::GetSocket()
{
	return _Socket;
}

void URocketLeagueStatsAPISocket::AppendToBuffer(const FString& Data)
{
	MessageBuffer.Append(Data);
}

FString URocketLeagueStatsAPISocket::ExtractCompleteMessage()
{
	int32 BraceCount = 0;
	int32 StartIdx = MessageBuffer.Find(TEXT("{"));
	
	if (StartIdx == INDEX_NONE)
	{
		return FString();
	}
	
	for (int32 i = StartIdx; i < MessageBuffer.Len(); i++)
	{
		if (MessageBuffer[i] == '{')
		{
			BraceCount++;
		}
		else if (MessageBuffer[i] == '}')
		{
			BraceCount--;
			if (BraceCount == 0)
			{
				FString CompleteMessage = MessageBuffer.Mid(StartIdx, i - StartIdx + 1);
				MessageBuffer.RemoveAt(0, i + 1);
				return CompleteMessage;
			}
		}
	}
	
	return FString();
}

void URocketLeagueStatsAPISocket::ClearBuffer()
{
	MessageBuffer.Empty();
}

bool URocketLeagueStatsAPISocket::HasCompleteJSON()
{
	int32 BraceCount = 0;
	int32 StartIdx = MessageBuffer.Find(TEXT("{"));
	
	if (StartIdx == INDEX_NONE)
	{
		return false;
	}
	
	for (int32 i = StartIdx; i < MessageBuffer.Len(); i++)
	{
		if (MessageBuffer[i] == '{')
		{
			BraceCount++;
		}
		else if (MessageBuffer[i] == '}')
		{
			BraceCount--;
			if (BraceCount == 0)
			{
				return true;
			}
		}
	}
	
	return false;
}

void URocketLeagueStatsAPISocket::StartBackgroundReading()
{
	if (ReaderThread != nullptr)
	{
		return;
	}

	if (_Socket == nullptr)
	{
		UE_LOG(LogRLSocket, Error, TEXT("Cannot start background reading: No socket"));
		return;
	}

	ReaderThread = new FSocketReaderThread(_Socket);
	ReaderThread->StartThread();
	
	UE_LOG(LogRLSocket, Log, TEXT("Background reading started"));
}

void URocketLeagueStatsAPISocket::StopBackgroundReading()
{
	if (ReaderThread != nullptr)
	{
		ReaderThread->Stop();
		delete ReaderThread;
		ReaderThread = nullptr;
		UE_LOG(LogRLSocket, Log, TEXT("Background reading stopped"));
	}
}

void URocketLeagueStatsAPISocket::Tick(float DeltaTime)
{
	if (ReaderThread != nullptr)
	{
		FString Message;
		while (ReaderThread->DequeueMessage(Message))
		{
			if (!Message.IsEmpty())
			{
				OnMessageReceived.Broadcast(Message);
			}
		}
		return;
	}

	if (!bAutoPolling || _Socket == nullptr)
	{
		return;
	}

	TimeSinceLastPoll += DeltaTime;
	if (TimeSinceLastPoll >= PollingInterval)
	{
		TimeSinceLastPoll = 0.0f;
		PollForData();
	}
}

bool URocketLeagueStatsAPISocket::IsTickable() const
{
	return (ReaderThread != nullptr) || (bAutoPolling && _Socket != nullptr);
}

TStatId URocketLeagueStatsAPISocket::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URocketLeagueStatsAPISocket, STATGROUP_Tickables);
}

void URocketLeagueStatsAPISocket::StartAutoPolling(float InPollingInterval)
{
	if (_Socket == nullptr)
	{
		return;
	}

	PollingInterval = InPollingInterval;
	bAutoPolling = true;
	TimeSinceLastPoll = 0.0f;
}

void URocketLeagueStatsAPISocket::StopAutoPolling()
{
	if (bAutoPolling)
	{
		bAutoPolling = false;
	}
}

void URocketLeagueStatsAPISocket::PollForData()
{
	if (_Socket == nullptr)
	{
		return;
	}

	ESocketConnectionState State = _Socket->GetConnectionState();
	if (State != SCS_Connected)
	{
		return;
	}

	uint32 PendingDataSize = 0;
	if (!_Socket->HasPendingData(PendingDataSize) || PendingDataSize == 0)
	{
		return;
	}

	const uint32 MaxReadSize = 8192;
	uint32 ReadSize = FMath::Min(PendingDataSize, MaxReadSize);
	
	TArray<uint8> BinaryData;
	BinaryData.SetNumUninitialized(ReadSize);
	
	int32 BytesRead = 0;
	if (!_Socket->Recv(BinaryData.GetData(), ReadSize, BytesRead, ESocketReceiveFlags::None))
	{
		return;
	}

	if (BytesRead <= 0)
	{
		return;
	}

	BinaryData.SetNum(BytesRead);
	BinaryData.Add(0);
	FString RawData = FString(UTF8_TO_TCHAR(BinaryData.GetData()));
	AppendToBuffer(RawData);

	while (HasCompleteJSON())
	{
		FString CompleteMessage = ExtractCompleteMessage();
	
		if (!CompleteMessage.IsEmpty())
		{
			OnMessageReceived.Broadcast(CompleteMessage);
		}
	}
}