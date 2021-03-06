/*
** $Id$
**
** Copyright © Quazaa Development Team, 2009-2013.
** This file is part of QUAZAA (quazaa.sourceforge.net)
**
** Quazaa is free software; this file may be used under the terms of the GNU
** General Public License version 3.0 or later as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.
**
** Quazaa is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
**
** Please review the following information to ensure the GNU General Public
** License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** You should have received a copy of the GNU General Public License version
** 3.0 along with Quazaa; if not, write to the Free Software Foundation,
** Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "queryhit.h"
#include "g2packet.h"
#include "Hashes/hash.h"

#include "debug_new.h"

CQueryHit::CQueryHit()
{
	m_pNext = 0;
	m_nObjectSize = 0;
	m_nCachedSources = 0;
	m_bIsPartial = 0;
	m_nPartialBytesAvailable = 0;
	m_bIsP2PIMCapable = false;
}
CQueryHit::CQueryHit(CQueryHit *pHit):
	m_pNext(NULL),
	m_pHitInfo(pHit->m_pHitInfo),
	m_lHashes(pHit->m_lHashes),
	m_sDescriptiveName(pHit->m_sDescriptiveName),
	m_sURL(pHit->m_sURL),
	m_sMetadata(pHit->m_sMetadata),
	m_nObjectSize(pHit->m_nObjectSize),
	m_nCachedSources(pHit->m_nCachedSources),
	m_bIsPartial(pHit->m_bIsPartial),
	m_nPartialBytesAvailable(pHit->m_nPartialBytesAvailable),
	m_sPreviewURL(pHit->m_sPreviewURL),
	m_bIsP2PIMCapable(pHit->m_bIsP2PIMCapable)

{
}

CQueryHit::~CQueryHit()
{
	//qDebug() << "CQueryHit destructor";

	if(m_pNext)
	{
		delete m_pNext;
	}
}

QueryHitInfo* CQueryHit::readInfo(G2Packet* pPacket, CEndPoint* pSender)
{
	// do a shallow parsing...

	QueryHitInfo* pHitInfo = new QueryHitInfo();

	bool bHaveHits = false;
	bool bHaveNA = false;
	bool bHaveGUID = false;

	try
	{
		char szType[9];
		quint32 nLength = 0, nNext = 0;
		bool bCompound = false;

		if(pSender)
		{
			pHitInfo->m_oNodeAddress = *pSender;
			pHitInfo->m_oSenderAddress = *pSender;
			bHaveNA = true;
		}

		while(pPacket->readPacket(&szType[0], nLength, &bCompound))
		{
			nNext = pPacket->m_nPosition + nLength;

			if(strcmp("H", szType) == 0 && bCompound)
			{
				bHaveHits = true;
				continue;
			}

			if(bCompound)
			{
				pPacket->skipCompound();
			}

			if(strcmp("NA", szType) == 0 && nLength >= 6)
			{
				CEndPoint oNodeAddr;
				pPacket->readHostAddress(&oNodeAddr, !(nLength >= 18));
				if(oNodeAddr.isValid())
				{
					pHitInfo->m_oNodeAddress = oNodeAddr;
					bHaveNA = true;
				}
			}
			else if(strcmp("GU", szType) == 0 && nLength >= 16)
			{
				QUuid oNodeGUID = pPacket->readGUID();
				if(!oNodeGUID.isNull())
				{
					pHitInfo->m_oNodeGUID = oNodeGUID;
					bHaveGUID = true;
				}
			}
			else if(strcmp("NH", szType) == 0 && nLength >= 6)
			{
				CEndPoint oNH;
				pPacket->readHostAddress(&oNH, !(nLength >= 18));
				if(oNH.isValid())
				{
					pHitInfo->m_lNeighbouringHubs.append(oNH);
				}
			}
			else if(strcmp("V", szType) == 0 && nLength >= 4)
			{
				pHitInfo->m_sVendor = pPacket->readString(4);
			}

			pPacket->m_nPosition = nNext;
		}
	}
	catch(...)
	{
		delete pHitInfo;
		return 0;
	}

	if(pPacket->getRemaining() < 17 || !bHaveHits || !bHaveNA || !bHaveGUID)
	{
		systemLog.postLog(LogSeverity::Debug, QString("Malformatted hit in CSearchManager %1 %2 %3 %4").arg(pPacket->getRemaining()).arg(bHaveHits).arg(bHaveNA).arg(bHaveGUID));
		//qDebug() << "Malformatted hit in CSearchManager" << pPacket->GetRemaining() << bHaveHits << bHaveNA << bHaveGUID;
		delete pHitInfo;
		return 0;
	}

	pHitInfo->m_nHops = pPacket->readByte();
	pHitInfo->m_oGUID = pPacket->readGUID();

	return pHitInfo;
}

CQueryHit* CQueryHit::readPacket(G2Packet* pPacket, QueryHitInfo* pHitInfo)
{
	if(!pPacket->m_bCompound)
	{
		return 0;
	}

	pPacket->m_nPosition = 0; // reset packet position

	bool bHaveHits = false;
	bool bFirstHit = true;

	CQueryHit* pThisHit = new CQueryHit();

	try
	{
		char szType[9], szTypeX[9];
		quint32 nLength = 0, nLengthX = 0, nNext = 0, nNextX = 0;
		bool bCompound = false;

		while(pPacket->readPacket(&szType[0], nLength, &bCompound))
		{
			nNext = pPacket->m_nPosition + nLength;

			if(strcmp("H", szType) == 0 && bCompound)
			{
				CQueryHit* pHit = (bFirstHit ? pThisHit : new CQueryHit());

				if(!bFirstHit)
				{
					CQueryHit* pPrevHit = pThisHit;
					while(pPrevHit->m_pNext != 0)
					{
						pPrevHit = pPrevHit->m_pNext;
					}
					pPrevHit->m_pNext = pHit;
				}

				bool bHaveSize = false;
				QByteArray baTemp;
				bool bHaveDN = false;
				bool bHaveURN = false;

				while(pPacket->m_nPosition < nNext && pPacket->readPacket(&szTypeX[0], nLengthX))
				{
					nNextX = pPacket->m_nPosition + nLengthX;

					if(strcmp("URN", szTypeX) == 0)
					{
						QString sURN;
						QByteArray hashBuff;
						sURN = pPacket->readString();

						if(nLengthX >= 44u && sURN.compare("bp") == 0)
						{
							hashBuff.resize(CHash::byteCount(CHash::SHA1));
							pPacket->read(hashBuff.data(), CHash::byteCount(CHash::SHA1));
							CHash* pHash = CHash::fromRaw(hashBuff, CHash::SHA1);
							if(pHash)
							{
								pHit->m_lHashes.append(*pHash);
								bHaveURN = true;
							}
							delete pHash;
							// TODO: Tiger
						}
						else if(nLengthX >= CHash::byteCount(CHash::SHA1) + 5u && sURN.compare("sha1") == 0)
						{
							hashBuff.resize(CHash::byteCount(CHash::SHA1));
							pPacket->read(hashBuff.data(), CHash::byteCount(CHash::SHA1));
							CHash* pHash = CHash::fromRaw(hashBuff, CHash::SHA1);
							if(pHash)
							{
								pHit->m_lHashes.append(*pHash);
								bHaveURN = true;
							}
							delete pHash;
						}

					}
					else if(strcmp("URL", szTypeX) == 0 && nLengthX)
					{
						// if url empty - try uri-res resolver or a node do not have this object
						// bez sensu...
						pHit->m_sURL = pPacket->readString();
					}
					else if(strcmp("DN", szTypeX) == 0)
					{
						if(bHaveSize)
						{
							pHit->m_sDescriptiveName = pPacket->readString(nLengthX);
						}
						else if(nLengthX > 4)
						{
							baTemp.resize(4);
							pPacket->read(baTemp.data(), 4);
							pHit->m_sDescriptiveName = pPacket->readString(nLengthX - 4);
						}

						bHaveDN = true;
					}
					else if(strcmp("MD", szTypeX) == 0)
					{
						pHit->m_sMetadata = pPacket->readString();
					}
					else if(strcmp("SZ", szTypeX) == 0 && nLengthX >= 4)
					{
						if(nLengthX >= 8)
						{
							if(!baTemp.isEmpty())
							{
								pHit->m_sDescriptiveName.prepend(baTemp);
							}
							pHit->m_nObjectSize = pPacket->readIntLE<quint64>();
							bHaveSize = true;
						}
						else if(nLengthX >= 4)
						{
							if(!baTemp.isEmpty())
							{
								pHit->m_sDescriptiveName.prepend(baTemp);
							}
							pHit->m_nObjectSize = pPacket->readIntLE<quint32>();
							bHaveSize = true;
						}
					}
					else if(strcmp("CSC", szTypeX) == 0 && nLengthX >= 2)
					{
						pHit->m_nCachedSources = pPacket->readIntLE<quint16>();
					}
					else if(strcmp("PART", szTypeX) == 0 && nLengthX >= 4)
					{
						pHit->m_bIsPartial = true;
						pHit->m_nPartialBytesAvailable = pPacket->readIntLE<quint32>();
					}
					pPacket->m_nPosition = nNextX;
				}

				if(!bHaveSize && baTemp.size() == 4)
				{
					pHit->m_nObjectSize = qFromLittleEndian(*(quint32*)baTemp.constData());
				}
				else
				{
					pHit->m_sDescriptiveName.prepend(baTemp);
				}

				if(bHaveURN && bHaveDN)
				{
					bFirstHit = false;
					bHaveHits = true;
				}
				else
				{
					if(!bFirstHit)
					{
						// może teraz się nie wywali...
						// można by było to lepiej zrobić...
						for(CQueryHit* pTest = pThisHit; pTest != 0; pTest = pTest->m_pNext)
						{
							if(pTest->m_pNext == pHit)
							{
								pTest->m_pNext = 0;
								break;
							}
						}
						delete pHit;
					}
				}
			}

			pPacket->m_nPosition = nNext;
		}
	}
	catch(...) // packet incomplete, packet error, parser takes care of stream end
	{
		systemLog.postLog(LogSeverity::Debug, "EXCEPTION IN QUERY HIT PARSING!");
		qDebug() << "EXCEPTION IN QUERY HIT PARSING!";
		if(pThisHit)
		{
			delete pThisHit;
		}
		//throw;
		return 0;
	}

	// we already know query hit informations, so don't reparse them
	if(!bHaveHits)
	{
		delete pThisHit;
		return 0;
	}

	CQueryHit* pHit = pThisHit;
	QSharedPointer<QueryHitInfo> pHitInfoS(pHitInfo);

	while(pHit != 0)
	{
		pHit->m_pHitInfo = pHitInfoS;

		pHit = pHit->m_pNext;
	}

	// TODO: sprawdzic poprawnosc hita... (Validate hit)

	pHit = pThisHit;

	while(pHit != 0)
	{
		pHit->resolveURLs();
		pHit = pHit->m_pNext;
	}

	return pThisHit;
}

void CQueryHit::resolveURLs()
{
	if(!m_sURL.isEmpty())
	{
		return;
	}

	if( m_lHashes.isEmpty() )
		return;

	m_sURL = QString("http://%1/uri-res/N2R?%2").arg(m_pHitInfo->m_oNodeAddress.toStringWithPort()).arg(m_lHashes[0].toURN());
}
bool CQueryHit::isValid(CQuery* pQuery)
{
	if(pQuery)
	{
		//
	}

	return true;
}

