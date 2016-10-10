/*
* Licensed to Green Energy Corp (www.greenenergycorp.com) under one or
* more contributor license agreements. See the NOTICE file distributed
* with this work for additional information regarding copyright ownership.
* Green Energy Corp licenses this file to you under the Apache License,
* Version 2.0 (the "License"); you may not use this file except in
* compliance with the License.  You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* This project was forked on 01/01/2013 by Automatak, LLC and modifications
* may have been made to this file. Automatak, LLC licenses these modifications
* to you under the terms of the License.
*/

#include "asiopal/TCPClient.h"

#include "asiopal/SocketHelpers.h"

namespace asiopal
{

TCPClient::TCPClient(
    const std::shared_ptr<StrandExecutor>& executor,
    const IPEndpoint& remote,
    const std::string& adapter
) :
	executor(executor),
	socket(executor->strand.get_io_service()),
	host(remote.address),
	adapter(adapter),
	remoteEndpoint(asio::ip::tcp::v4(), remote.port),
	localEndpoint(),
	resolver(executor->strand.get_io_service())
{}


bool TCPClient::Cancel()
{
	if (this->canceled || !this->connecting)
	{
		return false;
	}

	std::error_code ec;
	socket.cancel(ec);
	this->canceled = true;
	return true;
}

bool TCPClient::BeginConnect(const connect_callback_t& callback)
{
	if (connecting || canceled) return false;

	this->connecting = true;

	std::error_code ec;
	SocketHelpers::BindToLocalAddress(this->adapter, this->localEndpoint, this->socket, ec);

	if (ec)
	{
		return this->PostConnectError(callback, ec);
	}

	const auto address = asio::ip::address::from_string(this->host, ec);
	if (ec)
	{
		return this->PostConnectError(callback, ec);
		// TODO handle DNS resolution
	}
	else
	{
		remoteEndpoint.address(address);
		auto self = this->shared_from_this();
		auto cb = [self, callback](const std::error_code & ec)
		{
			self->connecting = false;
			if (!self->canceled)
			{
				callback(self->executor, std::move(self->socket), ec);
			}
		};

		socket.async_connect(remoteEndpoint, executor->strand.wrap(cb));
		return true;
	}
}

bool TCPClient::PostConnectError(const connect_callback_t& callback, const std::error_code& ec)
{
	auto self = this->shared_from_this();
	auto cb = [self, ec, callback]()
	{
		self->connecting = false;
		if (!self->canceled)
		{
			callback(self->executor, std::move(self->socket), ec);
		}
	};
	executor->strand.post(cb);
	return true;
}

}


