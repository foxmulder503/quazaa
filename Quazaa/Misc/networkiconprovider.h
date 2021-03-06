/*
** $Id: networkiconprovider.h 928 2012-02-15 17:19:00Z brov $
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

#ifndef NETWORKICONPROVIDER_H
#define NETWORKICONPROVIDER_H

// TODO: Provide country flags, too.

#include "types.h"

class QIcon;

/**
 * @brief The CNetworkIconProvider class can be used to obtain network icons. It optimizes memory
 * usage and HDD accesses using the QPixmapCache.
 */
class CNetworkIconProvider
{
private:
	/**
	 * @brief CNetworkIconProvider creates a new CNetworkIconProvider object.
	 */
	CNetworkIconProvider();

public:
	/**
	 * @brief icon can be used to get an apporpriate icon for a given DiscoveryProtocol.
	 * @param protocol
	 * @return The requested icon. Note that it internally uses shared data so you don't need to
	 *         worry about copying it around or requesting the same icon twice (in terms of memory
	 *         consumption).
	 */
	static QIcon icon(DiscoveryProtocol protocol);

	/**
	 * @brief icon can be used to get an apporpriate icon for a given TransferProtocol.
	 * @param protocol
	 * @return The requested icon. Note that it internally uses shared data so you don't need to
	 *         worry about copying it around or requesting the same icon twice (in terms of memory
	 *         consumption).
	 */
	static QIcon icon(TransferProtocol protocol);
};

#endif // NETWORKICONPROVIDER_H
