#include "Connection.h"

namespace netcode {

CConnection::CConnection()
{
	dataSent = 0;
	dataRecv = 0;
}

CConnection::~CConnection()
{
}

unsigned CConnection::GetDataReceived() const
{
	return dataRecv;
}

} // namespace netcode
