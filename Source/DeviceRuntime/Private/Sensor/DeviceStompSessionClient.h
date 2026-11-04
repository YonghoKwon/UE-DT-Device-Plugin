#pragma once

#include "IStompClient.h"

/** DTCore의 연결/세대/재구독을 그대로 쓰면서 기존 session-only CONNECT 인증을 전달한다.
 * 전역 DTCore 설정이나 디스크에 인증을 복사하지 않는 투명 위임 adapter이다.
 */
class FDeviceStompSessionClient final : public IStompClient
{
public:
	FDeviceStompSessionClient(TSharedPtr<IStompClient> InClient, FStompHeader InHeaders)
	    : Client(MoveTemp(InClient)), SessionHeaders(MoveTemp(InHeaders)) {}
	virtual void Connect(const FStompHeader& Headers) override
	{
		auto Effective = Headers;
		for (const auto& Pair : SessionHeaders)
			Effective.Add(Pair.Key, Pair.Value);
		Client->Connect(Effective);
	}
	virtual void Disconnect(const FStompHeader& Headers) override { Client->Disconnect(Headers); }
	virtual bool IsConnected() const override { return Client->IsConnected(); }
	virtual FStompSubscriptionId Subscribe(const FString& Destination, const FStompSubscriptionEvent& EventCallback, const FStompRequestCompleted& CompletionCallback) override { return Client->Subscribe(Destination, EventCallback, CompletionCallback); }
	virtual void Unsubscribe(FStompSubscriptionId Id, const FStompRequestCompleted& Callback) override { Client->Unsubscribe(Id, Callback); }
	virtual void Send(const FString& Destination, const FStompBuffer& Body, const FStompHeader& Headers, const FStompRequestCompleted& Callback) override { Client->Send(Destination, Body, Headers, Callback); }
	virtual FStompClientConnectedEvent& OnConnected() override { return Client->OnConnected(); }
	virtual FStompClientConnectionErrorEvent& OnConnectionError() override { return Client->OnConnectionError(); }
	virtual FStompClientErrorEvent& OnError() override { return Client->OnError(); }
	virtual FStompClientClosedEvent& OnClosed() override { return Client->OnClosed(); }

private:
	TSharedPtr<IStompClient> Client;
	FStompHeader SessionHeaders;
};
