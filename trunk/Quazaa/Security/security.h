#ifndef SECURITY_H
#define SECURITY_H

#include <list>
#include <map>
#include <queue>
#include <set>

#include <QDomDocument>
#include <QHostAddress>
#include <QMutex>
#include <QString>
#include <QUuid>

#include "file.h"
#include "securerule.h"
#include "time.h"

// Increment this if there have been made changes to the way of storing security rules.
#define SECURITY_CODE_VERSION	0
// History:
// 0 - Initial implementation

namespace security
{
	// typedef enum { tri_unknown, tri_true, tri_false } tristate;

	class CSecurity : public QObject
	{
		Q_OBJECT
		/* ================================================================ */
		/* ========================== Attributes ========================== */
		/* ================================================================ */
	public:
		QMutex					m_pSection;
		static const QString	xmlns;

		typedef enum
		{
			banSession, ban5Mins, ban30Mins, ban2Hours, banWeek, banMonth, banForever
		} BanLength;

	private:
		typedef std::pair< QString, CHashRule* > CHashPair;

		typedef std::list< CSecureRule*  > CSecurityRuleList;
		typedef std::list< CIPRangeRule* > CIPRangeRuleList;
		typedef std::list< CRegExpRule*  > CRegExpRuleList;
		typedef std::list< CContentRule* > CContentRuleList;

		typedef std::queue< CSecureRule* > CNewRulesQueue;

		typedef std::set< QString > CMissCache;

		typedef std::map< QString, CIPRule*        > CAddressRuleMap;
		typedef std::map< QString, CCountryRule*   > CCountryRuleMap;
		typedef std::map< QString, CUserAgentRule* > CUserAgentRuleMap;

		// Note: Using a multimap eliminates eventual problems of hash
		// collisions caused by weaker hashes like MD5 for example.
		typedef std::multimap< QString, CHashRule* > CHashRuleMap;

		typedef CSecurityRuleList::const_iterator CIterator;

		// contains all rules
		CSecurityRuleList	m_Rules;

		// Used to manage newly added rules during sanity check
		CSecurityRuleList	m_loadedAddressRules;
		CNewRulesQueue		m_newAddressRules;
		CSecurityRuleList	m_loadedHitRules;
		CNewRulesQueue		m_newHitRules;

		// IP rule miss cache
		CMissCache			m_Cache;

		// single IP blocking rules
		CAddressRuleMap		m_IPs;

		// multiple IP blocking rules
		CIPRangeRuleList	m_IPRanges;

		// country rules
		CCountryRuleMap		m_Countries;

		// hash rules
		CHashRuleMap		m_Hashes;

		// all other content rules
		CContentRuleList	m_Contents;

		// RegExp rules
		CRegExpRuleList		m_RegExpressions;

		// User agent rules
		CUserAgentRuleMap	m_UserAgents;

		// Expire Counters
		quint32				m_tLastRuleExpiryCheck;
		quint32				m_tLastMissCacheExpiryCheck;

		bool				m_bUseMissCache;
		bool				m_bIsLoading;		// true during import operations. Used to avoid unnecessary GUI updates.
		bool				m_bNewRulesLoaded;	// true if new rules for sanity check have been loaded.
		bool				m_bDelayedSanityCheckRequested;
		quint32				m_tSanityCheckRequestTime;

		bool				m_bSaved;			// true if current security manager state already saved to file, false otherwise

		bool				m_bDenyPolicy;
		// m_bDenyPolicy == false : everything but specifically blocked IPs is allowed (default)
		// m_bDenyPolicy == true  : everything but specifically allowed IPs is rejected
		// Note that the default policy is only applied to IP related rules, as everything
		// else does not make any sense.

	public:
		/* ================================================================ */
		/* ========================= Construction ========================= */
		/* ================================================================ */
		CSecurity();
		~CSecurity();

		/* ================================================================ */
		/* ========================== Operations ========================== */
		/* ================================================================ */
		inline unsigned int	getCount() const;

		inline bool		denyPolicy() const;
		void			setDenyPolicy(bool bDenyPolicy);

		bool			check(CSecureRule* pRule);
		void			add(CSecureRule* pRule);
		void			remove(CSecureRule* pRule);
		void			clear();
		void			expire();

		// Sanity check controll methods
		void			sanityCheck(bool bDelay = false);
		bool			loadNewRules();
		bool			clearNewRules();

		void			ban(const QHostAddress& oAddress, BanLength nBanLength, bool bMessage = true, const QString& strComment = "");
		void			ban(const CFile& oFile, BanLength nBanLength, bool bMessage = true, const QString& strComment = "");

		// Methods used during sanity check
		bool			isNewlyDenied(const QHostAddress& oAddress);
		bool			isNewlyDenied(/*const CQueryHit* pHit,*/ const QList<QString>& lQuery);

		bool			isDenied(const QHostAddress& oAddress, const QString &source = "Unknown");
		// This does not check for the hit IP to avoid double checking.
		bool			isDenied(/*const CQueryHit* pHit, */const QList<QString>& lQuery);

		// Checks the user agent to see if it's a GPL breaker, or other trouble-maker
		// We don't ban them, but also don't offer leaf slots to them.
		bool			isClientBad(const QString& sUserAgent) const;

		// Checks the user agent to see if it's a leecher client, or other banned client
		// Test new releases, and remove block if/when they are fixed.
		// Includes check of agent blocklist & agent security rules.
		bool			isAgentBlocked(const QString& sUserAgent);

		// Check the evil's G1/G2 vendor code
		bool			isVendorBlocked(const QString& sVendor) const;

		// Export/Import/Load/Save handlers
		bool			load();
		bool			save(bool bForceSaving = false);				// brov: this method should be called on exit with bForceSaving = true.
		bool			import(const QString& sPath);
		QDomDocument	toXML();
		bool			fromXML(const QDomDocument& oXMLroot);

	signals:
		void			ruleAdded(QSharedPointer<CSecureRule> pRule);
		void			ruleRemoved(QSharedPointer<CSecureRule> pRule);

	private:
		// This generates a read/write iterator from a read-only iterator.
		// TODO: Convince brov to help me make this template only require 1 class parameter,
		// as in fact T_it == T::iterator and T_const_it == T::const_iterator...
		template<class T, class T_it, class T_const_it> inline T_it getIterator(T container, T_const_it const_it)
		{
			T_it i( container.begin() );
			std::advance( i, std::distance< T_const_it >( i, const_it ) );
			return i;
		}

		bool			load(QString sPath);

		// this returns the first rule found. Note that there might be others, too.
		CIterator		getHash(const QList< CHash >& hashes) const;
		CIterator		getUUID(const QUuid& oUUID) const;

		void			remove(CIterator i);

		bool			isAgentDenied(const QString& strUserAgent);

		void			missCacheAdd(const QString& sIP);
		void			missCacheClear(const quint32& tNow);
		void			evaluateCacheUsage();				// determines whether it is logical to use the cache or not

		bool			isDenied(const QString& strContent);
		bool			isDenied(const CFile& oFile);
		bool			isDenied(const QList<QString>& lQuery, const QString& strContent);
	};

	unsigned int CSecurity::getCount() const
	{
		return m_Rules.size();
	}

	bool CSecurity::denyPolicy() const
	{
		return m_bDenyPolicy;
	}

}

extern security::CSecurity securityManager;

#endif // SECURITY_H
