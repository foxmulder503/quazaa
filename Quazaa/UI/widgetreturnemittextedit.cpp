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

#include "widgetreturnemittextedit.h"
#include "skinsettings.h"

#include <QShortcut>
#include <QDebug>

#if QT_VERSION >= 0x050000 
#include <QTextDocument> //For Qt::escape()
#endif

#ifdef _DEBUG
#include "debug_new.h"
#endif

WidgetReturnEmitTextEdit::WidgetReturnEmitTextEdit(QWidget *parent)
{
	Q_UNUSED(parent);
	resetHistoryIndex();
	m_oCompleter = new Completer(this);
	m_oCompleter->setWidget(this);
	m_oCompleter->setTextEdit(this);

	emitReturn = true;
	connect(this, SIGNAL(textChanged()), this, SLOT(onTextChanged()));

	setAttribute(Qt::WA_MacShowFocusRect, false);

	connect(this, SIGNAL(tabPressed()), m_oCompleter, SLOT(onTabPressed()));
	setSkin();
}

void WidgetReturnEmitTextEdit::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Return)
	{
		if (emitReturn)
		{
			emit returnPressed();
		} else {
			QTextEdit::keyPressEvent(event);
		}
	} else if (event->key() == Qt::Key_Up) {
		if (!m_lHistory.isEmpty())
		{
			if(m_iHistoryIndex < 1)
				m_iHistoryIndex = (m_lHistory.size() - 1);
			else
				--m_iHistoryIndex;
			event->ignore();
			setHtml(m_lHistory.at(m_iHistoryIndex));
		} else {
			event->ignore();
			QTextEdit::keyPressEvent(event);
		}
	} else if (event->key() == Qt::Key_Down) {
		if (!m_lHistory.isEmpty())
		{
			if(m_iHistoryIndex == -1 || m_iHistoryIndex == (m_lHistory.size() - 1))
				m_iHistoryIndex = 0;
			else
				++m_iHistoryIndex;
			event->ignore();
			setHtml(m_lHistory.at(m_iHistoryIndex));
		} else {
			event->ignore();
			QTextEdit::keyPressEvent(event);
		}
	} else {
		resetHistoryIndex();
		QTextEdit::keyPressEvent(event);
	}
}

void WidgetReturnEmitTextEdit::setEmitsReturn(bool shouldEmit)
{
	emitReturn = shouldEmit;
}

bool WidgetReturnEmitTextEdit::emitsReturn()
{
	return emitReturn;
}

Completer* WidgetReturnEmitTextEdit::completer() const
{
	return m_oCompleter;
}

QString WidgetReturnEmitTextEdit::textUnderCursor() const
{
	QTextCursor tc = textCursor();
	tc.select(QTextCursor::WordUnderCursor);

	// Save selected positions of current word.
	int selectionStart = tc.selectionStart();
	int selectionEnd = tc.selectionEnd();

	// If selection is at beginning of text edit then there can't be a slash to check for
	if (selectionStart == 0)
		return tc.selectedText();

	// Modify selection to include previous character
	tc.setPosition(selectionStart - 1, QTextCursor::MoveAnchor);
	tc.setPosition(selectionEnd, QTextCursor::KeepAnchor);

	// If previous character was / return current selection for command completion
	if(tc.selectedText().startsWith('/'))
		return tc.selectedText();
	else {
		// Else restore original selection and return for nick completion
		tc.setPosition(selectionStart, QTextCursor::MoveAnchor);
		tc.setPosition(selectionEnd, QTextCursor::KeepAnchor);
		return tc.selectedText();
	}
}

void WidgetReturnEmitTextEdit::onTextChanged()
{
	emit textChanged(toPlainText());
}

void WidgetReturnEmitTextEdit::setSkin()
{

}

bool WidgetReturnEmitTextEdit::focusNextPrevChild(bool next)
{
	if (!tabChangesFocus() && this->textInteractionFlags() & Qt::TextEditable)
	{
		emit tabPressed();
		return true;
	} else {
		return QAbstractScrollArea::focusNextPrevChild(next);
	}
}

void WidgetReturnEmitTextEdit::addHistory(QTextDocument* document)
{
	if(m_lHistory.count() > 49 && !m_lHistory.isEmpty())
	{
		m_lHistory.removeAt(0);
		m_lPlainTextHistory.removeAt(0);
	}
	if(!m_lPlainTextHistory.contains(document->toPlainText()))
	{
		m_lHistory.append(document->toHtml());
		m_lPlainTextHistory.append(document->toPlainText());
	}
	resetHistoryIndex();
}

void WidgetReturnEmitTextEdit::addHistory(QString* text)
{
	QTextDocument* document = new QTextDocument();
#if QT_VERSION >= 0x050000 
	document->setHtml(text->toHtmlEscaped());
#else
	QString escapeText = text->toLocal8Bit();
	document->setHtml(Qt::escape(escapeText));
#endif
	addHistory(document);
}

void WidgetReturnEmitTextEdit::resetHistoryIndex()
{
	m_iHistoryIndex = -1;
}