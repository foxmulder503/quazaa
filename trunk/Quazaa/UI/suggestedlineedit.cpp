/*
** $Id$
**
** Copyright © Quazaa Development Team, 2009-2012.
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

#include "suggestedlineedit.h"

#include <QLineEdit>
#include <QCompleter>
#include <QStandardItemModel>
#include <QSettings>
#include <QAbstractItemView>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkReply>
#include <QXmlStreamReader>
#include <QTimer>

#include <QDebug>

CSuggestedLineEdit::CSuggestedLineEdit(QLineEdit *lineEdit, QObject *parent) :
	QObject(parent),
	m_pLineEdit(lineEdit),
	m_pModel(new QStandardItemModel(parent)),
	m_nMaxRecent(10),
	m_bEnableExternal(false),
	m_pNetworkAccessManager(0)
{
	// Set up completer
	if( !lineEdit->completer() )
	{
		lineEdit->setCompleter(new QCompleter(lineEdit));
	}
	lineEdit->completer()->setModel(m_pModel);
	lineEdit->completer()->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
	connect(m_pLineEdit->completer(), SIGNAL(activated(QString)), this, SLOT(updateRecent()));
	connect(m_pLineEdit, SIGNAL(returnPressed()), this, SLOT(updateRecent()));
	connect(m_pLineEdit, SIGNAL(textChanged(const QString&)), this, SLOT(onTextChanged(QString)));

	// object name used to save/restore recent suggestions
	QString sObjName;

	QObject* parentObj = lineEdit->parent();
	do
	{
		sObjName.prepend("/").prepend(parentObj->objectName());
		parentObj = parentObj->parent();
	} while ( parentObj );

	sObjName.append(lineEdit->objectName());

	setObjectName(sObjName);

	load();
}

CSuggestedLineEdit::~CSuggestedLineEdit()
{
	save();
	delete m_pModel;
}

void CSuggestedLineEdit::load()
{
	QSettings s;

	s.beginGroup(objectName());
	m_lRecent = s.value("recent").toStringList();
	m_nMaxRecent = s.value("max", 10).toInt();
	s.endGroup();
}

void CSuggestedLineEdit::save()
{
	QSettings s;

	s.beginGroup(objectName());
	s.setValue("recent", m_lRecent);
	s.setValue("max", m_nMaxRecent);
	s.endGroup();
}

void CSuggestedLineEdit::setNetworkAccessManager(QNetworkAccessManager *pManager)
{
	m_pNetworkAccessManager = pManager;
}

void CSuggestedLineEdit::setEnableExternal(bool bEnable)
{
	if( m_bEnableExternal == bEnable )
		return;

	m_bEnableExternal = bEnable;
}
bool CSuggestedLineEdit::enableExternal()
{
	return m_bEnableExternal;
}

void CSuggestedLineEdit::setMaxRecentItems(uint nMax)
{
	m_nMaxRecent = nMax;
}

int CSuggestedLineEdit::maxRecentItems()
{
	return m_nMaxRecent;
}

void CSuggestedLineEdit::updateRecent()
{
	QString text = m_pLineEdit->text().toLower();

	if( m_lRecent.contains(text) )
	{
		m_lRecent.removeAt(m_lRecent.indexOf(text));
	}

	m_lRecent.prepend(text);

	while(m_lRecent.size() > m_nMaxRecent)
	{
		m_lRecent.removeLast();
	}
}

void CSuggestedLineEdit::getSuggestions()
{
	QString text = m_pLineEdit->text();
	if( m_bEnableExternal )
	{
		Q_ASSERT(m_pNetworkAccessManager);
		if(text.isEmpty() || !m_pNetworkAccessManager)
		{
			setupCompleter();
			return;
		}

		QUrl url(QLatin1String("http://www.google.com/complete/search"));
		url.addEncodedQueryItem(QUrl::toPercentEncoding(QLatin1String("q")),
								QUrl::toPercentEncoding(text));
		url.addQueryItem(QLatin1String("output"), QLatin1String("toolbar"));
		QNetworkRequest request(url);
		request.setAttribute(QNetworkRequest::User, text);
		QNetworkReply *reply = m_pNetworkAccessManager->get(request);
		connect(reply, SIGNAL(finished()), this, SLOT(finished()), Qt::QueuedConnection);
	}
	else
	{
		setupCompleter();
	}
}

void CSuggestedLineEdit::setupCompleter()
{
	m_pModel->clear();

	if( m_bEnableExternal && !m_lSuggested.isEmpty() )
	{
		if( m_pModel->rowCount() == 0 )
		{
			QStandardItem* pSuggestionsItem = new QStandardItem();
			pSuggestionsItem->setEnabled(false);
			pSuggestionsItem->setText(tr("Suggestions"));

			m_pModel->appendRow(pSuggestionsItem);
		}

		for(int i = 0; i < m_lSuggested.size(); ++i)
		{
			m_pModel->appendRow(new QStandardItem(m_lSuggested.at(i)));
		}
	}

	if( m_lRecent.isEmpty() )
	{
		QStandardItem* item = new QStandardItem(tr("No recent items"));
		item->setEnabled(false);
		m_pModel->appendRow(item);
	}
	else
	{
		QStandardItem* item = new QStandardItem(tr("Recent items"));
		item->setEnabled(false);
		m_pModel->appendRow(item);

		for(int i = 0; i < m_lRecent.size(); ++i)
		{
			QStandardItem* newItem = new QStandardItem(m_lRecent.at(i));
			m_pModel->appendRow(newItem);
		}
	}

	QAbstractItemView *view = m_pLineEdit->completer()->popup();
	view->setMinimumHeight(view->sizeHintForRow(0) * m_pModel->rowCount() + view->frameWidth() * 2);
	m_pLineEdit->completer()->complete();
}

void CSuggestedLineEdit::finished()
{
	QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
	if (!reply || reply->error() != QNetworkReply::NoError)
	{
		setupCompleter();
		return;
	}

	QStringList suggestionsList;
	QByteArray result = reply->readAll();
	QXmlStreamReader xml(result);
	while (!xml.atEnd()) {
		xml.readNext();
		if (xml.tokenType() == QXmlStreamReader::StartElement) {
			if (xml.name() == QLatin1String("suggestion")) {
				QStringRef str = xml.attributes().value(QLatin1String("data"));
				suggestionsList << str.toString();
			}
		}
	}
	QString searchText = reply->request().attribute(QNetworkRequest::User).toString();
	reply->deleteLater();

	m_lSuggested.clear();

	if(searchText == m_pLineEdit->text())
	{
		m_lSuggested = suggestionsList;
	}

	setupCompleter();
}

void CSuggestedLineEdit::onTextChanged(const QString &text)
{
	if(text.isEmpty())
	{
		m_lSuggested.clear();
		m_pModel->clear();
		setupCompleter();
	}
	else
	{
		QTimer::singleShot(250, this, SLOT(getSuggestions()));
	}
}

