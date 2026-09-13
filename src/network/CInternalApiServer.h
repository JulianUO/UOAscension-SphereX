/**
 * @file CInternalApiServer.h
 * @brief Localhost HTTP API for external login server integration.
 */

#ifndef _INC_CINTERNALAPISERVER_H
#define _INC_CINTERNALAPISERVER_H

#include "../common/sphere_library/CSString.h"
#include "../sphere/threads.h"
#include "CSocket.h"

class CInternalApiServer : public AbstractSphereThread
{
private:
	CSocket m_listenSocket;

	void handleClient(SOCKET clientSocket);
	bool authorizeRequest(lpctstr authHeader) const;
	bool handlePostSessions(lpctstr body, CSString& responseBody);
	bool handleGetStatus(CSString& responseBody) const;
	static bool extractJsonString(lpctstr json, lpctstr key, CSString& value);
	static bool extractJsonDword(lpctstr json, lpctstr key, dword& value);
	static bool extractJsonInt(lpctstr json, lpctstr key, int& value);
	static void sendHttpResponse(SOCKET clientSocket, int statusCode, lpctstr statusText, lpctstr contentType, lpctstr body);

public:
	CInternalApiServer();
	~CInternalApiServer() override;

	void onStart() override;
	void tick() override;
	bool shouldExit() noexcept override;
	void waitForClose() override;
};

#endif // _INC_CINTERNALAPISERVER_H
