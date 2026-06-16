// Copyright (c) 2026 Sumatras Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Containers/Queue.h"
#include "Templates/Atomic.h"

class FSocket;

class FSocketReaderThread : public FRunnable
{
public:
	FSocketReaderThread(FSocket* InSocket);
	virtual ~FSocketReaderThread();

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

	void StartThread();
	bool HasMessages() const;
	bool DequeueMessage(FString& OutMessage);

private:
	FSocket* Socket = nullptr;
	FRunnableThread* Thread = nullptr;

	TAtomic<bool> bStopRequested{ false };

	FString MessageBuffer;
	TQueue<FString, EQueueMode::Mpsc> MessageQueue;
	FCriticalSection BufferLock;

	bool ExtractCompleteMessage(FString& OutMessage);
};