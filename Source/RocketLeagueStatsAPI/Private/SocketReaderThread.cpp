// Copyright (c) 2026 Sumatras Studios. All Rights Reserved.

#include "SocketReaderThread.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogSocketReader, Log, All);

FSocketReaderThread::FSocketReaderThread(FSocket* InSocket)
	: Socket(InSocket)
	, Thread(nullptr)
{
}

FSocketReaderThread::~FSocketReaderThread()
{
	if (Thread != nullptr)
	{
		Thread->Kill(true);
		delete Thread;
		Thread = nullptr;
	}
}

bool FSocketReaderThread::Init()
{
	UE_LOG(LogSocketReader, Log, TEXT("Socket reader thread initialized"));
	return true;
}

void FSocketReaderThread::StartThread()
{
	if (Thread == nullptr)
	{
		Thread = FRunnableThread::Create(this, TEXT("SocketReaderThread"), 0, TPri_Normal);
	}
}

uint32 FSocketReaderThread::Run()
{
	UE_LOG(LogSocketReader, Log, TEXT("Socket reader thread started - BLOCKING mode (true subscription)"));

	if (Socket)
	{
		Socket->SetNonBlocking(false);
		UE_LOG(LogSocketReader, Log, TEXT("Socket set to BLOCKING mode"));
	}

	while (!bStopRequested)
	{
		if (Socket == nullptr)
		{
			break;
		}

		ESocketConnectionState State = Socket->GetConnectionState();
		if (State != SCS_Connected)
		{
			UE_LOG(LogSocketReader, Warning, TEXT("Socket not connected"));
			break;
		}

		TArray<uint8> BinaryData;
		BinaryData.SetNumUninitialized(8192);

		int32 BytesRead = 0;

		if (!Socket->Recv(BinaryData.GetData(), 8192, BytesRead, ESocketReceiveFlags::None))
		{
			if (bStopRequested)
			{
				break;
			}

			ESocketErrors LastError =
				ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLastErrorCode();

			UE_LOG(LogSocketReader, Error,
				TEXT("Socket recv error: %d"),
				(int32)LastError);

			break;
		}

		if (BytesRead == 0)
		{
			UE_LOG(LogSocketReader, Log, TEXT("Connection closed"));
			break;
		}

		BinaryData.SetNum(BytesRead);
		BinaryData.Add(0);

		FString RawData = FString(UTF8_TO_TCHAR(BinaryData.GetData()));

		{
			FScopeLock Lock(&BufferLock);
			MessageBuffer.Append(RawData);
		}

		FString CompleteMessage;

		while (true)
		{
			{
				FScopeLock Lock(&BufferLock);

				if (!ExtractCompleteMessage(CompleteMessage))
				{
					break;
				}
			}

			if (!CompleteMessage.IsEmpty())
			{
				MessageQueue.Enqueue(CompleteMessage);
			}
		}
	}

	UE_LOG(LogSocketReader, Log, TEXT("Socket reader thread stopped"));
	return 0;
}

void FSocketReaderThread::Stop()
{
	bStopRequested = true;

	if (Socket)
	{
		Socket->Close();
	}
}

void FSocketReaderThread::Exit()
{
	UE_LOG(LogSocketReader, Log, TEXT("Socket reader thread exiting"));
}

bool FSocketReaderThread::HasMessages() const
{
	return !MessageQueue.IsEmpty();
}

bool FSocketReaderThread::DequeueMessage(FString& OutMessage)
{
	return MessageQueue.Dequeue(OutMessage);
}

bool FSocketReaderThread::ExtractCompleteMessage(FString& OutMessage)
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
				OutMessage = MessageBuffer.Mid(StartIdx, i - StartIdx + 1);
				MessageBuffer.RemoveAt(0, i + 1);
				return true;
			}
		}
	}

	return false;
}