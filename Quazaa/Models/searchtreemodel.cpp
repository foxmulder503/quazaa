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

#include "searchtreemodel.h"
#include <QFileInfo>
#include "systemlog.h"
#include "geoiplist.h"
#include "commonfunctions.h"
#include "Hashes/hash.h"
#include "fileiconprovider.h"
#include "networkiconprovider.h"

#include "debug_new.h"

using namespace common;

SearchTreeModel::SearchTreeModel() :
    m_pIconProvider( new CFileIconProvider ),
    m_pFilter( new SearchFilter )
{
	QList<QVariant> rootData;
	rootData << "File"
	         << "Extension"
	         << "Size"
	         << "Rating"
	         << "Status"
	         << "Host/Count"
	         << "Speed"
	         << "Client"
	         << "Country";
	rootItem = new SearchTreeItem( rootData );
	nFileCount = 0;
}

SearchTreeModel::~SearchTreeModel()
{
	clear();
	delete rootItem;
	delete m_pIconProvider;
}

bool SearchTreeModel::isRoot(QModelIndex index)
{
	SearchTreeItem* item;

	if ( !index.isValid() )
	{
		item = rootItem;
	}
	else
	{
		item = static_cast<SearchTreeItem*>( index.internalPointer() );
	}

	if ( item == rootItem )
	{
		return true;
	}

	return false;
}

int SearchTreeModel::columnCount(const QModelIndex& parent) const
{
	if ( parent.isValid() )
	{
		return static_cast<SearchTreeItem*>( parent.internalPointer() )->columnCount();
	}
	else
	{
		return rootItem->columnCount();
	}
}

QVariant SearchTreeModel::data(const QModelIndex& index, int role) const
{
	if ( !index.isValid() )
	{
		return QVariant();
	}

	SearchTreeItem* item = static_cast<SearchTreeItem*>( index.internalPointer() );

	if ( role == Qt::DecorationRole )
	{
		if ( !index.column() )
		{
			if ( item->parent() == rootItem )
				return m_pIconProvider->icon( item->data( 1 ).toString().prepend( "." ) );
			else
				return item->HitData.iNetwork;
		}

		if ( index.column() == 8 )
		{
			return item->HitData.iCountry;
		}
	}

	if ( role == Qt::DisplayRole )
	{
		return item->data( index.column() );
	}

	return QVariant();
}

Qt::ItemFlags SearchTreeModel::flags(const QModelIndex& index) const
{
	if ( !index.isValid() )
	{
		return 0;
	}

	return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant SearchTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if ( orientation == Qt::Horizontal && role == Qt::DisplayRole )
	{
		return rootItem->data( section );
	}

	return QVariant();
}

QModelIndex SearchTreeModel::index(int row, int column, const QModelIndex& parent) const
{
	if ( !hasIndex( row, column, parent ) )
	{
		return QModelIndex();
	}

	SearchTreeItem* parentItem;

	if ( !parent.isValid() )
	{
		parentItem = rootItem;
	}
	else
	{
		parentItem = static_cast<SearchTreeItem*>( parent.internalPointer() );
	}

	SearchTreeItem* childItem = parentItem->child( row );
	if ( childItem )
	{
		return createIndex( row, column, childItem );
	}
	else
	{
		return QModelIndex();
	}
}

QModelIndex SearchTreeModel::parent(const QModelIndex& index) const
{
	if ( !index.isValid() )
	{
		return QModelIndex();
	}

	SearchTreeItem* childItem = static_cast<SearchTreeItem*>( index.internalPointer() );
	SearchTreeItem* parentItem = childItem->parent();

	if ( parentItem == rootItem )
	{
		return QModelIndex();
	}

	return createIndex( parentItem->row(), 0, parentItem );
}

int SearchTreeModel::rowCount(const QModelIndex& parent) const
{
	SearchTreeItem* parentItem;
	if ( parent.column() > 0 )
	{
		return 0;
	}

	if ( !parent.isValid() )
	{
		parentItem = rootItem;
	}
	else
	{
		parentItem = static_cast<SearchTreeItem*>( parent.internalPointer() );
	}

	return parentItem->childCount();
}

void SearchTreeModel::setupModelData(const QStringList& lines, SearchTreeItem* parent)
{
	QList<SearchTreeItem*> parents;
	QList<int> indentations;
	parents << parent;
	indentations << 0;

	int number = 0;

	while ( number < lines.count() )
	{
		int position = 0;
		while ( position < lines[number].length() )
		{
			if ( lines[number].mid( position, 1 ) != " " )
			{
				break;
			}
			++position;
		}

		QString lineData = lines[number].mid( position ).trimmed();

		if ( !lineData.isEmpty() )
		{
			// Read the column data from the rest of the line.
			QStringList columnStrings = lineData.split( "\t", QString::SkipEmptyParts );
			QList<QVariant> columnData;
			for ( int column = 0; column < columnStrings.count(); ++column )
			{
				columnData << columnStrings[column];
			}

			if ( position > indentations.last() )
			{
				// The last child of the current parent is now the new parent
				// unless the current parent has no children.

				if ( parents.last()->childCount() > 0 )
				{
					parents << parents.last()->child( parents.last()->childCount() - 1 );
					indentations << position;
				}
			}
			else
			{
				while ( position < indentations.last() && parents.count() > 0 )
				{
					parents.pop_back();
					indentations.pop_back();
				}
			}

			// Append a new item to the current parent's list of children.
			parents.last()->appendChild( new SearchTreeItem( columnData, parents.last() ) );
		}

		++number;
	}
}

void SearchTreeModel::clear()
{
	beginRemoveRows( QModelIndex(), 0, rootItem->childCount() );
	//qDebug() << "clearSearch passing to rootItem";
	rootItem->clearChildren();
	endRemoveRows();

	QModelIndex idx1 = index( 0, 0, QModelIndex() );
	QModelIndex idx2 = index( rootItem->childCount(), 10, QModelIndex() );
	emit dataChanged( idx1, idx2 );
}

void SearchTreeModel::addQueryHit(QueryHitSharedPtr pHit)
{
	CQueryHit* pHit2 = pHit.data();

	while ( pHit2 != 0 )
	{
		int existingSearch = -1;

		foreach ( CHash pHash, pHit2->m_lHashes )
		{
			existingSearch = rootItem->find( rootItem, pHash );
			if ( existingSearch != -1 )
				break;
		}

		if ( existingSearch == -1 )
		{
			QFileInfo fileInfo( pHit2->m_sDescriptiveName );

			++geoIP.m_nDebugOldCalls;
			QString sCountry = pHit2->m_pHitInfo.data()->m_oNodeAddress.country();

			beginInsertRows( QModelIndex(), rootItem->childCount(), rootItem->childCount() );
			QList<QVariant> lParentData;
			lParentData << fileInfo.completeBaseName()         // File name
			            << fileInfo.suffix()                   // Extension
			            << formatBytes( pHit2->m_nObjectSize ) // Size
			            << ""                                  // Rating
			            << ""                                  // Status
			            << 1                                   // Host/Count
			            << ""                                  // Speed
			            << ""                                  // Client
			            << "";                                 // Country
			SearchTreeItem* m_oParentItem = new SearchTreeItem( lParentData, rootItem );
			m_oParentItem->HitData.lHashes << pHit2->m_lHashes;
			QList<QVariant> m_lChildData;
			m_lChildData << fileInfo.completeBaseName()
			             << fileInfo.suffix()
						 << formatBytes(pHit2->m_nObjectSize)
			             << ""
			             << ""
			             << pHit2->m_pHitInfo.data()->m_oNodeAddress.toString()
			             << ""
						 << common::vendorCodeToName(pHit2->m_pHitInfo.data()->m_sVendor)
			             << geoIP.countryNameFromCode(sCountry);
			SearchTreeItem* m_oChildItem = new SearchTreeItem(m_lChildData, m_oParentItem);
			m_oChildItem->HitData.lHashes << pHit2->m_lHashes;
			m_oChildItem->HitData.iNetwork = CNetworkIconProvider::icon(dpG2);
			m_oChildItem->HitData.iCountry = QIcon(":/Resource/Flags/" + sCountry.toLower() + ".png");

			QueryHitSharedPtr pHitX(new CQueryHit(pHit2));
			m_oChildItem->HitData.pQueryHit = pHitX;

			rootItem->appendChild(m_oParentItem);
			m_oParentItem->appendChild(m_oChildItem);
			endInsertRows();
			nFileCount = rootItem->childCount();
		}
		else if(!rootItem->child(existingSearch)->duplicateCheck(rootItem->child(existingSearch), pHit2->m_pHitInfo.data()->m_oNodeAddress.toString()))
		{
			QModelIndex idxParent = index(existingSearch, 0, QModelIndex());
			QFileInfo fileInfo(pHit2->m_sDescriptiveName);

			++geoIP.m_nDebugOldCalls;
			QString sCountry = pHit2->m_pHitInfo.data()->m_oNodeAddress.country();

			beginInsertRows(idxParent, rootItem->child(existingSearch)->childCount(), rootItem->child(existingSearch)->childCount());
			QList<QVariant> m_lChildData;
			m_lChildData << fileInfo.completeBaseName()
			             << fileInfo.suffix()
						 << formatBytes(pHit2->m_nObjectSize)
			             << ""
			             << ""
			             << pHit2->m_pHitInfo.data()->m_oNodeAddress.toString()
			             << ""
						 << common::vendorCodeToName(pHit2->m_pHitInfo.data()->m_sVendor)
			             << geoIP.countryNameFromCode(sCountry);
			SearchTreeItem* m_oChildItem = new SearchTreeItem(m_lChildData, rootItem->child(existingSearch));
			m_oChildItem->HitData.lHashes << pHit2->m_lHashes;
			m_oChildItem->HitData.iNetwork = CNetworkIconProvider::icon(dpG2);
			m_oChildItem->HitData.iCountry = QIcon(":/Resource/Flags/" + sCountry.toLower() + ".png");

			QueryHitSharedPtr pHitX(new CQueryHit(pHit2));
			m_oChildItem->HitData.pQueryHit = pHitX;

			rootItem->child(existingSearch)->appendChild(m_oChildItem);
			rootItem->child(existingSearch)->updateHitCount(rootItem->child(existingSearch)->childCount());
			endInsertRows();
		}

		pHit2 = pHit2->m_pNext;
	}

	emit updateStats();

	QModelIndex idx1 = index(0, 0, QModelIndex());
	QModelIndex idx2 = index(rootItem->childCount(), 10, QModelIndex());
	emit dataChanged(idx1, idx2);
	emit sort();
}

SearchTreeItem::SearchTreeItem(const QList<QVariant> &data, SearchTreeItem* parent)
{
	parentItem = parent;
	itemData = data;
}

SearchTreeItem::~SearchTreeItem()
{
	qDeleteAll(childItems);
}

void SearchTreeItem::appendChild(SearchTreeItem* item)
{
	item->parentItem = this;
	childItems.append(item);
}

void SearchTreeItem::clearChildren()
{
	qDeleteAll(childItems);
	childItems.clear();
}

SearchTreeItem* SearchTreeItem::child(int row)
{
	return childItems.value(row);
}

int SearchTreeItem::childCount() const
{
	return childItems.count();
}

int SearchTreeItem::columnCount() const
{
	return itemData.count();
}

int SearchTreeItem::find(SearchTreeItem* containerItem, CHash& pHash)
{
	for ( int index = 0; index < containerItem->childItems.size(); ++index )
	{
		if ( containerItem->child( index )->HitData.lHashes.contains( pHash ) )
		{
			return index;
		}
	}
	return -1;
}

QVariant SearchTreeItem::data(int column) const
{
	return itemData.value(column);
}

SearchTreeItem* SearchTreeItem::parent()
{
	return parentItem;
}

int SearchTreeItem::row() const
{
	if(parentItem)
	{
		return parentItem->childItems.indexOf(const_cast<SearchTreeItem*>(this));
	}

	return 0;
}

void SearchTreeItem::updateHitCount(int count)
{
	itemData[5] = count;
}

bool SearchTreeItem::duplicateCheck(SearchTreeItem* containerItem, QString ip)
{
	for(int index = 0; index < containerItem->childItems.size(); ++index)
	{
		if(containerItem->child(index)->data(5).toString() == ip)
		{
			return true;
		}
	}
	return false;
}

SearchTreeItem * SearchTreeModel::itemFromIndex(QModelIndex index)
{
	Q_ASSERT(index.model() == this);

	if(index.isValid())
	{
		QModelIndex idxThis = index;

		while( parent(idxThis).isValid() )
		{
			idxThis = parent(idxThis);
		}

		SearchTreeItem* pThis = static_cast<SearchTreeItem*>(idxThis.internalPointer());

		Q_ASSERT(pThis != NULL);

		return pThis;
	}

	return NULL;
}

SearchFilter::SearchFilter() :
    m_sMatchString( "" ),
    m_bRegExp( false ),
    m_nMinSize( 0 ),
    m_nMaxSize( 18446744073709551615 ), // max value of 64 bit int
    m_nMinSources( 0 ),
    m_bBusy( true ),
    m_bFirewalled( true ),
    m_bUnstable( true ),
    m_bDRM( true ),
    m_bSuspicious( true ),
    m_bNonMatching( true ),
    m_bExistsInLibrary( true ),
    m_bBogus( true ),
    m_bAdult( true )
{
}

bool SearchFilter::operator==(const SearchFilter& rOther)
{
	return m_sMatchString     == rOther.m_sMatchString     &&
	       m_bRegExp          == rOther.m_bRegExp          &&
	       m_nMinSize         == rOther.m_nMinSize         &&
	       m_nMaxSize         == rOther.m_nMaxSize         &&
	       m_nMinSources      == rOther.m_nMinSources      &&
	       m_bBusy            == rOther.m_bBusy            &&
	       m_bFirewalled      == rOther.m_bFirewalled      &&
	       m_bUnstable        == rOther.m_bUnstable        &&
	       m_bDRM             == rOther.m_bDRM             &&
	       m_bSuspicious      == rOther.m_bSuspicious      &&
	       m_bNonMatching     == rOther.m_bNonMatching     &&
	       m_bExistsInLibrary == rOther.m_bExistsInLibrary &&
	       m_bBogus           == rOther.m_bBogus           &&
	       m_bAdult           == rOther.m_bAdult;
}

bool SearchFilter::operator!=(const SearchFilter& rOther)
{
	return !operator==( rOther );
}

// smaller amount of files
bool SearchFilter::operator<(const SearchFilter& rOther)
{
	return m_nMinSize         >  rOther.m_nMinSize         ||
	       m_nMaxSize         <  rOther.m_nMaxSize         ||
	       m_nMinSources      >  rOther.m_nMinSources      ||
	      !m_bBusy            && rOther.m_bBusy            ||
	      !m_bFirewalled      && rOther.m_bFirewalled      ||
	      !m_bUnstable        && rOther.m_bUnstable        ||
	      !m_bDRM             && rOther.m_bDRM             ||
	      !m_bSuspicious      && rOther.m_bSuspicious      ||
	      !m_bNonMatching     && rOther.m_bNonMatching     ||
	      !m_bExistsInLibrary && rOther.m_bExistsInLibrary ||
	      !m_bBogus           && rOther.m_bBogus           ||
	      !m_bAdult           && rOther.m_bAdult;
}

// bigger amount of files
bool SearchFilter::operator>(const SearchFilter& rOther)
{
	return m_nMinSize         <   rOther.m_nMinSize         ||
	       m_nMaxSize         >   rOther.m_nMaxSize         ||
	       m_nMinSources      <   rOther.m_nMinSources      ||
	       m_bBusy            && !rOther.m_bBusy            ||
	       m_bFirewalled      && !rOther.m_bFirewalled      ||
	       m_bUnstable        && !rOther.m_bUnstable        ||
	       m_bDRM             && !rOther.m_bDRM             ||
	       m_bSuspicious      && !rOther.m_bSuspicious      ||
	       m_bNonMatching     && !rOther.m_bNonMatching     ||
	       m_bExistsInLibrary && !rOther.m_bExistsInLibrary ||
	       m_bBogus           && !rOther.m_bBogus           ||
	       m_bAdult           && !rOther.m_bAdult;
}

