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

#include "downloads.h"
#include "download.h"
#include "downloadsource.h"
#include "transfers.h"

#include "quazaasettings.h"

#include <QDir>
#include <QFile>

#include "debug_new.h"

CDownloads Downloads;

CDownloads::CDownloads(QObject *parent) :
	QObject(parent)
{
	qRegisterMetaType<CDownload*>("CDownload*");
	qRegisterMetaType<CDownloadSource*>("CDownloadSource*");
	qRegisterMetaType<CDownload::DownloadState>("CDownload::DownloadState");
}

void CDownloads::add(CQueryHit *pHit)
{
	ASSUME_LOCK( Downloads.m_pSection );

	CDownload* pDownload = new CDownload( pHit );
	pDownload->moveToThread( &TransfersThread );
	m_lDownloads.append( pDownload );
	pDownload->saveState();
	systemLog.postLog( LogSeverity::Notice, Components::Downloads,
	                   qPrintable( tr( "Queued download job for %s" ) ),
	                   qPrintable( pDownload->m_sDisplayName ) );
	emit downloadAdded( pDownload );
}

bool CDownloads::exists(CDownload *pDownload)
{
	return (m_lDownloads.indexOf(pDownload) != -1);
}

void CDownloads::start()
{
	QMutexLocker l(&m_pSection);

	QDir d(quazaaSettings.Downloads.IncompletePath);

	if( !d.exists() )
		d.mkpath(quazaaSettings.Downloads.IncompletePath);

	if( d.isReadable() )
	{
		QStringList files = d.entryList(QStringList() << "*.!qd");

		foreach(QString f, files)
		{
			QFile file(quazaaSettings.Downloads.IncompletePath + "/" + f);

			if( file.exists() && file.open(QFile::ReadOnly) )
			{
				CDownload* pDownload = new CDownload();
				QDataStream stream(&file);

				stream >> *pDownload;

				pDownload->moveToThread(&TransfersThread);
				m_lDownloads.append(pDownload);
				emit downloadAdded(pDownload);
				systemLog.postLog( LogSeverity::Notice, Components::Downloads,
				                   qPrintable( tr( "Loaded download: %s" ) ),
				                   qPrintable( pDownload->m_sDisplayName ) );
			}
		}
	}
}

void CDownloads::stop()
{
	QMutexLocker l(&m_pSection);

	foreach( CDownload* pDownload, m_lDownloads )
	{
		if( pDownload->isModified() )
		{
			pDownload->saveState();
		}

		delete pDownload;
	}

	m_lDownloads.clear();
}

void CDownloads::emitDownloads()
{
	QMutexLocker l(&m_pSection);

	foreach( CDownload* pDl, m_lDownloads )
	{
		emit downloadAdded(pDl);
	}
}

void CDownloads::onTimer()
{
	if(m_lDownloads.isEmpty())
		return;

	QMutexLocker l(&m_pSection);

	int nActive = 0, nQueued = 0, nTransfers = 0;

	foreach(CDownload* pDownload, m_lDownloads)
	{
		switch(pDownload->m_nState)
		{
			case CDownload::dsQueued:
				nQueued++;
				break;
			case CDownload::dsPaused:
				break;
			case CDownload::dsPending:
			case CDownload::dsSearching:
			case CDownload::dsDownloading:
				nActive++;
				nTransfers += pDownload->transfersCount();
				break;
			case CDownload::dsVerifying:
			case CDownload::dsMoving:
			case CDownload::dsFileError:
			case CDownload::dsCompleted:
				break;

		}
	}

	int nTransfersLeft = quazaaSettings.Downloads.MaxTransfers - nTransfers;

	foreach(CDownload* pDownload, m_lDownloads)
	{
		if( pDownload->m_nState == CDownload::dsPending )
		{
			if( false /* starved? */ )
			{
				// stop it
			}
			else if( pDownload->sourceCount() < quazaaSettings.Downloads.MinSources /* and not too early */)
			{
				// run search
			}
		}
		else if( pDownload->m_nState == CDownload::dsQueued )
		{
			if( nActive < quazaaSettings.Downloads.MaxFiles )
			{
				systemLog.postLog(LogSeverity::Information, QString(tr("Starting download: %1")).arg(pDownload->m_sDisplayName));
				pDownload->start();
				nActive++;
			}
		}

		if( pDownload->canDownload() && nTransfersLeft > 0 )
		{
			int nAllow = qMin(3, (nTransfersLeft / (nActive + 1)));
			nTransfersLeft -= pDownload->startTransfers(nAllow);
		}
	}
}

