#include "../common/CLog.h"
#include "../game/CServer.h"
#include "../game/CServerConfig.h"
#include "../game/clients/CSessionRegistry.h"
#include "../sphere/ProfileTask.h"
#include "CInternalApiServer.h"

#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/select.h>
#include <unistd.h>
#endif

namespace
{
	constexpr int kMaxRequestSize = 8192;

	lpctstr skipHttpBody(lpctstr request, int contentLength)
	{
		lpctstr body = strstr(request, "\r\n\r\n");
		if (!body)
			return nullptr;
		body += 4;
		if (contentLength <= 0)
			return body;
		const lpctstr requestEnd = request + strlen(request);
		if ((requestEnd - body) < contentLength)
			return nullptr;
		return body;
	}

	lpctstr findHeaderValue(lpctstr request, lpctstr headerName)
	{
		const size_t nameLen = strlen(headerName);
		lpctstr line = request;
		while (line && *line)
		{
			lpctstr eol = strstr(line, "\r\n");
			if (!eol)
				break;
			if (_strnicmp(line, headerName, nameLen) == 0 && line[nameLen] == ':')
			{
				lpctstr value = line + nameLen + 1;
				while (*value == ' ')
					++value;
				return value;
			}
			line = eol + 2;
			if (line[0] == '\r' && line[1] == '\n')
				break;
		}
		return nullptr;
	}
}

CInternalApiServer::CInternalApiServer() : AbstractSphereThread("T_InternalApi", ThreadPriority::Idle)
{
	m_listenSocket.Close();
}

CInternalApiServer::~CInternalApiServer()
{
	m_listenSocket.Close();
}

void CInternalApiServer::onStart()
{
	AbstractSphereThread::onStart();

	if (g_Cfg.m_iInternalApiPort <= 0)
		return;

	if (!m_listenSocket.Create())
	{
		g_Log.Event(LOGL_ERROR | LOGM_INIT, "Internal API: failed to create listen socket.\n");
		m_listenSocket.Close();
		return;
	}

	int opt = 1;
	setsockopt(m_listenSocket.GetSocket(), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

	CSocketAddress bindAddr(SOCKET_LOCAL_ADDRESS, static_cast<word>(g_Cfg.m_iInternalApiPort));
	if (m_listenSocket.Bind(bindAddr) < 0 || m_listenSocket.Listen(8) < 0)
	{
		g_Log.Event(LOGL_ERROR | LOGM_INIT, "Internal API: failed to bind/listen on port %d.\n", g_Cfg.m_iInternalApiPort);
		m_listenSocket.Close();
		return;
	}

	g_Log.Event(LOGL_EVENT | LOGM_INIT, "Internal API listening on 127.0.0.1:%d\n", g_Cfg.m_iInternalApiPort);
	if (g_Cfg.m_sLoginSharedSecret.IsEmpty())
	{
		g_Log.Event(LOGL_WARN | LOGM_INIT,
			"Internal API: LoginSharedSecret is empty. External login-server session registration will fail (HTTP 401).\n"
			"Set LoginSharedSecret in " SPHERE_FILE ".ini to match shared_secret in login-server config.json.\n");
	}
	else
	{
		g_Log.Event(LOGM_INIT, "Internal API: login shared secret configured.\n");
	}
}

void CInternalApiServer::sendHttpResponse(SOCKET clientSocket, int statusCode, lpctstr statusText, lpctstr contentType, lpctstr body)
{
	tchar buffer[4096];
	const int bodyLen = body ? static_cast<int>(strlen(body)) : 0;
	const int headerLen = snprintf(buffer, sizeof(buffer),
		"HTTP/1.1 %d %s\r\n"
		"Content-Type: %s\r\n"
		"Content-Length: %d\r\n"
		"Connection: close\r\n"
		"\r\n",
		statusCode, statusText, contentType, bodyLen);
	if (headerLen > 0)
		send(clientSocket, buffer, headerLen, 0);
	if (bodyLen > 0)
		send(clientSocket, body, bodyLen, 0);
}

bool CInternalApiServer::authorizeRequest(lpctstr authHeader) const
{
	if (g_Cfg.m_sLoginSharedSecret.IsEmpty())
		return false;
	if (!authHeader)
		return false;

	lpctstr token = authHeader;
	while (*token == ' ')
		++token;
	if (_strnicmp(token, "Bearer ", 7) == 0)
		token += 7;
	while (*token == ' ')
		++token;

	const lpctstr pszSecret = g_Cfg.m_sLoginSharedSecret;
	const size_t secretLen = strlen(pszSecret);
	if (strncmp(token, pszSecret, secretLen) != 0)
		return false;
	const char ch = token[secretLen];
	return ch == '\0' || ch == '\r' || ch == '\n' || ch == ' ' || ch == '\t';
}

bool CInternalApiServer::extractJsonString(lpctstr json, lpctstr key, CSString& value)
{
	if (!json || !key)
		return false;

	tchar pattern[64];
	snprintf(pattern, sizeof(pattern), "\"%s\"", key);
	lpctstr pos = strstr(json, pattern);
	if (!pos)
		return false;
	pos = strchr(pos + strlen(pattern), '"');
	if (!pos)
		return false;
	++pos;
	lpctstr end = strchr(pos, '"');
	if (!end)
		return false;
	value.CopyLen(pos, static_cast<int>(end - pos));
	return !value.IsEmpty();
}

bool CInternalApiServer::extractJsonDword(lpctstr json, lpctstr key, dword& value)
{
	if (!json || !key)
		return false;

	tchar pattern[64];
	snprintf(pattern, sizeof(pattern), "\"%s\"", key);
	lpctstr pos = strstr(json, pattern);
	if (!pos)
		return false;
	pos = strchr(pos, ':');
	if (!pos)
		return false;
	++pos;
	while (*pos == ' ')
		++pos;
	value = static_cast<dword>(strtoul(pos, nullptr, 10));
	return true;
}

bool CInternalApiServer::extractJsonInt(lpctstr json, lpctstr key, int& value)
{
	dword tmp = 0;
	if (!extractJsonDword(json, key, tmp))
		return false;
	value = static_cast<int>(tmp);
	return true;
}

bool CInternalApiServer::handlePostSessions(lpctstr body, CSString& responseBody)
{
	CSString account;
	dword authId = 0;
	dword clientVersion = 0;
	dword reportedVersion = 0;
	int ttlSeconds = 30;

	if (!extractJsonString(body, "account", account) || !extractJsonDword(body, "auth_id", authId))
	{
		responseBody = "{\"ok\":false,\"error\":\"invalid_json\"}";
		return false;
	}

	extractJsonDword(body, "client_version", clientVersion);
	extractJsonDword(body, "reported_version", reportedVersion);
	extractJsonInt(body, "ttl_seconds", ttlSeconds);

	if (!CSessionRegistry::get().RegisterSession(authId, account, clientVersion, reportedVersion, ttlSeconds))
	{
		responseBody = "{\"ok\":false,\"error\":\"register_failed\"}";
		return false;
	}

	responseBody = "{\"ok\":true}";
	return true;
}

bool CInternalApiServer::handleGetStatus(CSString& responseBody) const
{
	const size_t players = g_Serv.StatGet(SERV_STAT_CLIENTS);
	const int maxPlayers = maximum(g_Cfg.m_iClientsMax, 1);
	const int percentFull = static_cast<int>(minimum((players * 100) / static_cast<size_t>(maxPlayers), static_cast<size_t>(100)));

	CSString shardName = g_Cfg.m_sShardDisplayName;
	if (shardName.IsEmpty())
		shardName = g_Serv.GetName();

	responseBody.Format(
		"{\"ok\":true,\"name\":\"%s\",\"players\":%u,\"max_players\":%d,\"percent_full\":%d}",
		static_cast<lpctstr>(shardName),
		static_cast<uint>(players),
		maxPlayers,
		percentFull);
	return true;
}

void CInternalApiServer::handleClient(SOCKET clientSocket)
{
	char request[kMaxRequestSize];
	const int received = recv(clientSocket, request, sizeof(request) - 1, 0);
	if (received <= 0)
	{
		CLOSESOCKET(clientSocket);
		return;
	}
	request[received] = '\0';

	const bool isPost = (strncmp(request, "POST ", 5) == 0);
	const bool isGet = (strncmp(request, "GET ", 4) == 0);
	const lpctstr authHeader = findHeaderValue(request, "Authorization");
	CSString responseBody;

	if (isGet && strstr(request, "GET /internal/v1/status") != nullptr)
	{
		if (!authorizeRequest(authHeader))
		{
			sendHttpResponse(clientSocket, 401, "Unauthorized", "application/json", "{\"ok\":false,\"error\":\"unauthorized\"}");
			CLOSESOCKET(clientSocket);
			return;
		}
		handleGetStatus(responseBody);
		sendHttpResponse(clientSocket, 200, "OK", "application/json", responseBody);
		CLOSESOCKET(clientSocket);
		return;
	}

	if (!isPost || strstr(request, "POST /internal/v1/sessions") == nullptr)
	{
		sendHttpResponse(clientSocket, 404, "Not Found", "application/json", "{\"ok\":false,\"error\":\"not_found\"}");
		CLOSESOCKET(clientSocket);
		return;
	}

	if (!authorizeRequest(authHeader))
	{
		sendHttpResponse(clientSocket, 401, "Unauthorized", "application/json", "{\"ok\":false,\"error\":\"unauthorized\"}");
		CLOSESOCKET(clientSocket);
		return;
	}

	int contentLength = 0;
	const lpctstr clHeader = findHeaderValue(request, "Content-Length");
	if (clHeader)
		contentLength = atoi(clHeader);

	const lpctstr body = skipHttpBody(request, contentLength);
	if (!body)
	{
		sendHttpResponse(clientSocket, 400, "Bad Request", "application/json", "{\"ok\":false,\"error\":\"missing_body\"}");
		CLOSESOCKET(clientSocket);
		return;
	}

	const bool ok = handlePostSessions(body, responseBody);
	sendHttpResponse(clientSocket, ok ? 201 : 400, ok ? "Created" : "Bad Request", "application/json", responseBody);
	CLOSESOCKET(clientSocket);
}

void CInternalApiServer::tick()
{
	if (!m_listenSocket.IsOpen())
		return;

	fd_set readfds;
	FD_ZERO(&readfds);
	int fdCount = 0;
	AddSocketToSet(readfds, m_listenSocket.GetSocket(), fdCount);

	timeval timeout {};
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

#ifdef _WIN32
	const int ready = select(0, &readfds, nullptr, nullptr, &timeout);
#else
	const int maxFd = static_cast<int>(m_listenSocket.GetSocket()) + 1;
	const int ready = select(maxFd, &readfds, nullptr, nullptr, &timeout);
#endif
	if (ready <= 0)
		return;

	CSocketAddress clientAddr;
	const SOCKET clientSocket = m_listenSocket.Accept(clientAddr);
	if (clientSocket == INVALID_SOCKET || clientSocket < 0)
		return;

	const ProfileTask task(PROFILE_NETWORK_RX);
	handleClient(clientSocket);
}

bool CInternalApiServer::shouldExit() noexcept
{
	if (!m_listenSocket.IsOpen() && g_Cfg.m_iInternalApiPort > 0)
		return true;
	return AbstractSphereThread::shouldExit();
}

void CInternalApiServer::waitForClose()
{
	m_listenSocket.Close();
	AbstractSphereThread::waitForClose();
}
