/*
* Copyright (C) 2008-2011 J-P Nurmi jpnurmi@gmail.com
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#include "messageview.h"
#include "session.h"
#include "completer.h"
#include "dialogircsettings.h"
#include <QStringListModel>
#include <QShortcut>
#include <QKeyEvent>
#include <QDebug>
#include <QDesktopServices>
#include <QApplication>
#include <irccommand.h>
#include <ircutil.h>
#include <irc.h>

#include "quazaasettings.h"
#include "quazaaglobals.h"
#include "commonfunctions.h"
#include "chatconverter.h"
#include "quazaasysinfo.h"

QStringListModel* MessageView::MessageViewData::commandModel = 0;

MessageView::MessageView(IrcSession* session, QWidget* parent) :
	QWidget(parent)
{
	d.setupUi(this);

	d.m_bIsStatusChannel = false;

	closeButton = new QToolButton(this);
	closeButton->setIcon(QIcon(":/Resource/Generic/Exit.png"));
	connect(closeButton, SIGNAL(clicked()), this, SLOT(part()));

	d.chatInput = new WidgetChatInput(0, true);
	d.horizontalLayoutChatInput->addWidget(d.chatInput, 1);

	setFocusProxy(d.chatInput->textEdit());
	d.textBrowser->installEventFilter(this);
	d.textBrowser->viewport()->installEventFilter(this);
	connect(d.textBrowser, SIGNAL(anchorClicked(QUrl)), this, SLOT(followLink(QUrl)));

	QTextCharFormat format;
	format.setFontStyleHint(QFont::TypeWriter);
	d.textBrowser->setCurrentCharFormat(format);

	d.labelTopic->setTextFormat(Qt::RichText);
	d.labelTopic->setVisible(false);
	connect(d.labelTopic, SIGNAL(linkActivated(QString)), this, SLOT(followLink(QString)));

	d.formatter.setHighlights(QStringList(session->nickName()));
	d.formatter.setMessageFormat("class='message'");
	d.formatter.setEventFormat("class='event'");
	d.formatter.setNoticeFormat("class='notice'");
	d.formatter.setActionFormat("class='action'");
	d.formatter.setUnknownFormat("class='unknown'");
	d.formatter.setHighlightFormat("class='highlight'");

	d.session = session;
	d.userModel = new IrcUserListModel();
	connect(&d.parser, SIGNAL(customCommand(QString,QStringList)), this, SLOT(onCustomCommand(QString,QStringList)));

	if (!d.commandModel)
	{
		CommandParser::addCustomCommand("CONNECT", "(<host> <port>)");
		CommandParser::addCustomCommand("QUERY", "<user> <message>");
		CommandParser::addCustomCommand("MSG", "<user> <message>");
		CommandParser::addCustomCommand("TELL", "<user> <message>");
		CommandParser::addCustomCommand("SETTINGS", "");
		CommandParser::addCustomCommand("JOIN", "<channel>");
		CommandParser::addCustomCommand("J", "<channel>");
		CommandParser::addCustomCommand("PART", "");
		CommandParser::addCustomCommand("SYSINFO", "");
		CommandParser::addCustomCommand("NS", "<nick service command (try help)>");
		CommandParser::addCustomCommand("CS", "<chanel service command (try help)>");
		CommandParser::addCustomCommand("HS", "<host service command (try help)>");
		CommandParser::addCustomCommand("MS", "<memo service command (try help)>");
		CommandParser::addCustomCommand("BS", "<bot service command (try help)>");
		CommandParser::addCustomCommand("OS", "<operator service command (try help)>");
		CommandParser::addCustomCommand("NICKSERV", "<nick service command (try help)>");
		CommandParser::addCustomCommand("CHANSERV", "<chanel service command (try help)>");
		CommandParser::addCustomCommand("HOSTSERV", "<host service command (try help)>");
		CommandParser::addCustomCommand("MEMOSERV", "<memo service command (try help)>");
		CommandParser::addCustomCommand("BOTSERV", "<bot service command (try help)>");
		CommandParser::addCustomCommand("OPERSERV", "<operator service command (try help)>");

		QStringList prefixedCommands;
		foreach (const QString& command, CommandParser::availableCommands())
			prefixedCommands += "/" + command;

		d.commandModel = new QStringListModel(qApp);
		d.commandModel->setStringList(prefixedCommands);
	}

	d.chatInput->textEdit()->completer()->setTabModel(d.userModel);
	d.chatInput->textEdit()->completer()->setSlashModel(d.commandModel);

	connect(d.chatInput, SIGNAL(messageSent(QTextDocument*)), this, SLOT(onSend(QTextDocument*)));
	connect(d.chatInput, SIGNAL(messageSent(QString)), this, SLOT(onSend(QString)));
	connect(d.chatInput->textEdit(), SIGNAL(textChanged(QString)), this, SLOT(showHelp(QString)));

	d.chatInput->helpLabel()->hide();
	d.searchEditor->setTextEdit(d.textBrowser);

	QShortcut* shortcut = new QShortcut(Qt::Key_Escape, this);
	connect(shortcut, SIGNAL(activated()), this, SLOT(onEscPressed()));

	connect(&quazaaSettings, SIGNAL(chatSettingsChanged()), this, SLOT(applySettings()));
	applySettings();
}

MessageView::~MessageView()
{
}

QString MessageView::receiver() const
{
	return d.receiver;
}

void MessageView::setReceiver(const QString& receiver)
{
	d.receiver = receiver;
}

void MessageView::setStatusChannel(bool statusChannel)
{
	d.m_bIsStatusChannel = statusChannel;
}

QAbstractItemModel *MessageView::userList()
{
	return d.userModel;
}

bool MessageView::isChannelView() const
{
	if (d.receiver.isEmpty())
		return false;

	switch (d.receiver.at(0).unicode())
	{
		case '#':
		case '&':
		case '!':
		case '+':
			return true;
		default:
			return false;
	}
}

bool MessageView::isStatusChannel() const
{
	return d.m_bIsStatusChannel;
}

void MessageView::showHelp(const QString& text, bool error)
{
	QString syntax;
	if (text == "/")
	{
		QStringList commands = CommandParser::availableCommands();
		syntax = commands.join(" ");
	}
	else if (text.startsWith('/'))
	{
		QStringList words = text.mid(1).split(' ');
		QString command = words.value(0);
		QStringList suggestions = CommandParser::suggestedCommands(command, words.mid(1));
		if (suggestions.count() == 1)
			syntax = CommandParser::syntax(suggestions.first());
		else
			syntax = suggestions.join(" ");

		if (syntax.isEmpty() && error)
			syntax = tr("Unknown command '%1'").arg(command.toUpper());
	}

	d.chatInput->helpLabel()->setVisible(!syntax.isEmpty());
	QPalette pal;
	if (error)
		pal.setColor(QPalette::WindowText, Qt::red);
	d.chatInput->helpLabel()->setPalette(pal);
	d.chatInput->helpLabel()->setText(syntax);
}

void MessageView::appendMessage(const QString& message)
{
	if (!message.isEmpty())
	{
		// workaround the link activation merge char format bug
		QString copy = message;
		if (copy.endsWith("</a>"))
			copy += " ";

		d.textBrowser->append(copy);
	}
}

bool MessageView::eventFilter(QObject* receiver, QEvent* event)
{
	Q_UNUSED(receiver);
	if (event->type() == QEvent::KeyPress)
	{
		QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
		// for example:
		// - Ctrl+C goes to the browser
		// - Ctrl+V goes to the line edit
		// - Shift+7 ("/") goes to the line edit
		switch (keyEvent->key())
		{
			case Qt::Key_Shift:
			case Qt::Key_Control:
			case Qt::Key_Meta:
			case Qt::Key_Alt:
			case Qt::Key_AltGr:
				break;
			default:
				if (!keyEvent->matches(QKeySequence::Copy))
				{
					QApplication::sendEvent(d.chatInput->textEdit(), keyEvent);
					d.chatInput->textEdit()->setFocus();
					return true;
				}
				break;
		}
	}
	return false;
}

void MessageView::onEscPressed()
{
	d.chatInput->helpLabel()->hide();
	d.searchEditor->hide();
	setFocus(Qt::OtherFocusReason);
}

void MessageView::onSend(const QString& text)
{
	if (isStatusChannel() && !text.startsWith("/"))
	{
		QString modify = text;
		modify.prepend(tr("/quote "));
		onSend(modify);
	} else {
		IrcCommand* cmd = d.parser.parseCommand(d.receiver, text);
		if (cmd)
		{
			if (cmd->type() == IrcCommand::Quote)
			{
				QString rawMessage = cmd->toString();
				if(isStatusChannel())
					appendMessage(d.formatter.formatRaw(rawMessage));
				else
					emit appendRawMessage(d.formatter.formatRaw(rawMessage));
			}
			d.session->sendCommand(cmd);
			d.sentCommands.insert(cmd->type());

			if (cmd->type() == IrcCommand::Message || cmd->type() == IrcCommand::CtcpAction)
			{
				IrcMessage* msg = IrcMessage::fromCommand(d.session->nickName(), cmd);
				receiveMessage(msg);
				delete msg;
			}
			else if (cmd->type() == IrcCommand::Quit)
			{
				emit aboutToQuit();
			}
		}
		else if (d.parser.hasError())
		{
			showHelp(text, true);
		}
	}
}

void MessageView::onSend(QTextDocument *message)
{
	CChatConverter *converter = new CChatConverter(message);
	onSend(converter->toIrc());
}

void MessageView::part()
{
	if(!isChannelView())
	{
		if(isStatusChannel())
		{
			emit partView(this);
		} else {
			emit closeQuery(this);
		}
	} else {
		emit partView(this);
	}
}

void MessageView::applySettings()
{
	d.formatter.setTimeStamp(quazaaSettings.Chat.TimeStamp);
	d.textBrowser->document()->setMaximumBlockCount(quazaaSettings.Chat.MaxBlockCount);

	QString backgroundColor = quazaaSettings.Chat.Colors.value(IrcColorType::Background);
	d.textBrowser->setStyleSheet(QString("QTextBrowser { background-color: %1 }").arg(backgroundColor));

	d.textBrowser->document()->setDefaultStyleSheet(
		QString(
			".highlight { color: %1 }"
			".message   { color: %2 }"
			".notice    { color: %3 }"
			".action    { color: %4 }"
			".event     { color: %5 }"
		).arg(quazaaSettings.Chat.Colors.value((IrcColorType::Highlight)))
		 .arg(quazaaSettings.Chat.Colors.value((IrcColorType::Message)))
		 .arg(quazaaSettings.Chat.Colors.value((IrcColorType::Notice)))
		 .arg(quazaaSettings.Chat.Colors.value((IrcColorType::Action)))
		 .arg(quazaaSettings.Chat.Colors.value((IrcColorType::Event))));
}

void MessageView::receiveMessage(IrcMessage* message)
{
	bool append = true;
	bool hilite = false;
	bool matches = false;

	switch (message->type())
	{
	case IrcMessage::Join:
		append = quazaaSettings.Chat.Messages.value(IrcMessageType::Joins);
		hilite = quazaaSettings.Chat.Highlights.value(IrcMessageType::Joins);
		break;
	case IrcMessage::Kick:
		append = quazaaSettings.Chat.Messages.value(IrcMessageType::Kicks);
		hilite = quazaaSettings.Chat.Highlights.value(IrcMessageType::Kicks);
		break;
	case IrcMessage::Mode:
	{
		append = quazaaSettings.Chat.Messages.value(IrcMessageType::Modes);
		hilite = quazaaSettings.Chat.Highlights.value(IrcMessageType::Modes);
		IrcModeMessage* modeMessage = static_cast<IrcModeMessage*>(message);
		d.userModel->updateUserMode(modeMessage->mode(), modeMessage->argument());
		break;
	}
	case IrcMessage::Nick:
		append = quazaaSettings.Chat.Messages.value(IrcMessageType::Nicks);
		hilite = quazaaSettings.Chat.Highlights.value(IrcMessageType::Nicks);
		break;
	case IrcMessage::Notice:
		matches = static_cast<IrcNoticeMessage*>(message)->message().contains(d.session->nickName());
		hilite = true;
		break;
	case IrcMessage::Part:
		append = quazaaSettings.Chat.Messages.value(IrcMessageType::Parts);
		hilite = quazaaSettings.Chat.Highlights.value(IrcMessageType::Parts);
		break;
	case IrcMessage::Private:
		matches = !isChannelView() || static_cast<IrcPrivateMessage*>(message)->message().contains(d.session->nickName());
		hilite = true;
		break;
	case IrcMessage::Quit:
		append = quazaaSettings.Chat.Messages.value(IrcMessageType::Quits);
		hilite = quazaaSettings.Chat.Highlights.value(IrcMessageType::Quits);
		break;
	case IrcMessage::Topic:
		append = quazaaSettings.Chat.Messages.value(IrcMessageType::Topics);
		hilite = quazaaSettings.Chat.Highlights.value(IrcMessageType::Topics);
		d.labelTopic->setVisible(true);
		d.labelTopic->setText(d.formatter.formatTopicOnly(static_cast<IrcTopicMessage*>(message)));
		break;
	case IrcMessage::Unknown:
		qWarning() << "unknown:" << message;
		append = false;
		break;
	case IrcMessage::Invite:
	case IrcMessage::Ping:
	case IrcMessage::Pong:
	case IrcMessage::Error:
		break;
	case IrcMessage::Numeric: {
			IrcNumericMessage* numeric = static_cast<IrcNumericMessage*>(message);
			if (numeric->code() == Irc::RPL_ENDOFNAMES && d.sentCommands.contains(IrcCommand::Names))
			{
				QString names = prettyNames(d.formatter.currentNames(), 6);
				appendMessage(d.formatter.formatMessage(message));
				appendMessage(names);
				d.sentCommands.remove(IrcCommand::Names);
				return;
			}
			if (numeric->code() == Irc::RPL_TOPIC)
			{
				d.labelTopic->setVisible(true);
				d.labelTopic->setText(d.formatter.formatTopicOnly(static_cast<IrcNumericMessage*>(message)));
			}
			break;
		}
	}

	if (matches)
		emit alert(this, true);
	else if (hilite)
		emit highlight(this, true);
	if (append)
		appendMessage(d.formatter.formatMessage(message));
}

void MessageView::addUser(const QString& user)
{
	// TODO: this is far from optimal
	QString modelessUser = user;
	QString modes;

	for(int i = 0; i  < d.session->prefixes().size(); i++)
	{
		if(user.startsWith(d.session->prefixes().at(i)))
		{
			modes = d.session->prefixToMode(d.session->prefixes().at(i));
			modelessUser.remove(0,1);
		}
	}

	d.userModel->addUser(modelessUser, modes);
}

void MessageView::removeUser(const QString& user)
{
	// TODO: this is far from optimal
	d.userModel->removeUser(user);
}

void MessageView::onCustomCommand(const QString& command, const QStringList& params)
{
	if (command == "QUERY")
	{
		if (!params.value(1).isEmpty())
		{
			QStringList message = params.mid(1);
			emit appendQueryMessage(params.value(0), message.join(" "));
		}
		else
			emit query(params.value(0));
	}
	else if (command == "MSG")
	{
		onCustomCommand("QUERY", params);
	}
	else if (command == "TELL")
	{
		onCustomCommand("QUERY", params);
	}
	else if (command == "SETTINGS")
	{
		DialogIrcSettings dlgSettings(qApp->activeWindow());
		dlgSettings.exec();
	}
	else if (command == "CONNECT")
		QMetaObject::invokeMethod(window(), "connectTo", Q_ARG(QString, params.value(0)), params.count() > 1 ? Q_ARG(quint16, params.value(1).toInt()) : QGenericArgument());
	else if (command == "JOIN")
		emit join(params.value(0));
	else if (command == "J")
		emit join(params.value(0));
	else if (command == "SYSINFO")
	{
		QuazaaSysInfo *sysInfo = new QuazaaSysInfo();
		onSend(tr("Application:%1 %2 OS:%3 Qt Version:%4").arg(QApplication::applicationName(), QuazaaGlobals::APPLICATION_VERSION_STRING(), sysInfo->osVersionToString(), qVersion()));
		//onSend(tr("CPU:%1 Cores:%2 Memory:%3").arg(QApplication::applicationName(), QuazaaGlobals::APPLICATION_VERSION_STRING()));
	}
	else if (command == "NS")
	{
		QStringList arguments = params;
		arguments.prepend("NickServ");
		onCustomCommand("QUERY", arguments);
	}
	else if (command == "NICKSERV")
		onCustomCommand("NS", params);

	else if (command == "CS")
	{
		QStringList arguments = params;
		arguments.prepend("ChanServ");
		onCustomCommand("QUERY", arguments);
	}
	else if (command == "CHANSERV")
		onCustomCommand("CS", params);

	else if (command == "HS")
	{
		QStringList arguments = params;
		arguments.prepend("HostServ");
		onCustomCommand("QUERY", arguments);
	}
	else if (command == "HOSTSERV")
		onCustomCommand("HS", params);

	else if (command == "MS")
	{
		QStringList arguments = params;
		arguments.prepend("MemoServ");
		onCustomCommand("QUERY", arguments);
	}
	else if (command == "MEMOSERV")
		onCustomCommand("MS", params);

	else if (command == "BS")
	{
		QStringList arguments = params;
		arguments.prepend("BotServ");
		onCustomCommand("QUERY", arguments);
	}
	else if (command == "BOTSERV")
		onCustomCommand("BS", params);

	else if (command == "OS")
	{
		QStringList arguments = params;
		arguments.prepend("OperServ");
		onCustomCommand("QUERY", arguments);
	}
	else if (command == "OPERSERV")
		onCustomCommand("OS", params);
	else if (command == "PART")
		part();
}

QString MessageView::prettyNames(const QStringList& names, int columns)
{
	QString message;
	message += "<table>";
	for (int i = 0; i < names.count(); i += columns)
	{
		message += "<tr>";
		for (int j = 0; j < columns; ++j)
			message += "<td>" + MessageFormatter::colorize(names.value(i+j)) + "&nbsp;</td>";
		message += "</tr>";
	}
	message += "</table>";
	return message;
}

void MessageView::followLink(const QString &link)
{
	if(link.startsWith("#"))
	{

	} else {
		QDesktopServices::openUrl(link);
	}
}

void MessageView::followLink(const QUrl &link)
{
	followLink(link.toString());
}